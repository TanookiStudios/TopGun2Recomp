/*
 * topgun2_extras.c - Top Gun: The Second Mission's additions to the NESRecomp
 * cycle host (runner/cyc/cyc_host_extras.h): its own menu rows, its own
 * config.ini [Game] values, and per-frame work.
 *
 * Contains no ROM bytes. Everything here is applied to the running machine.
 */
#include "cyc_host_extras.h"
#include "hw_internal.h"   /* hw.ram: the 2 KB of CPU work RAM */
#include "cpu6502.h"     /* cpu.do_reset: the console's Reset button */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef CYC_WITH_RECOMP_UI
#include "recomp_runtime_ui.h"
#endif

extern uint8_t hw_apu_mute_mask;   /* patches/0002: bit i silences APU channel i */

enum { CH_COUNT = 5 };
static const char *const CH_KEYS[CH_COUNT] = {
    "MutePulse1", "MutePulse2", "MuteTriangle", "MuteNoise", "MuteDMC",
};

/* Cheats. Each one undoes a drop of exactly one in a counter, so it never
 * touches the byte while the game boots or uses it for something else
 * (holding $45 from power-on freezes the title). Addresses: notes/ram-map.md. */
typedef struct { const char *key, *option; uint16_t addr; uint8_t slots; } Cheat;
static const Cheat CHEATS[] = {
    { "CheatLives",    "lives",    0x045, 1 },   /* lives left */
    /* One counter per missile type at $68+type ($57 = selected type); the
     * firing code does LDY $57 / LDA $0068,Y / SBC #1 / STA $0068,Y. */
    { "CheatMissiles", "missiles", 0x068, 3 },
    /* $440 = the player's plane: 0x40 flying, 0xE1 missile closing, 0xCx hit
 * (0xC8 and 0xC2 seen: the low bits are the kind of hit).
     * The crash code at $8B81 reads the 0xCx two frames after the hit lands
     * ($52 = 2 -> $50 = 6 at $F452), so turning it back to 0x40 at the end of
     * the frame cancels the hit. slots = 0: handled in apply_cheats. */
    { "CheatNoHit", "nohit", 0x440, 0 },
};
enum { CHEAT_NOHIT = 2 };
enum { CHEAT_COUNT = sizeof CHEATS / sizeof CHEATS[0] };

typedef struct {
    uint8_t mute;     /* bit per channel, CH_KEYS order */
    uint8_t cheats;   /* bit per CHEATS entry */
    uint8_t start;    /* START_NAMES index: which mission a new game begins on */
} Settings;

/* $48 is the stage. Briefings appear at stages 0, 2 and 5 (missions 1-3);
 * the stages between are later legs of the same mission (notes/ram-map.md). */
static const char *const START_NAMES[] = { "Mission 1", "Mission 2", "Mission 3" };
static const uint8_t START_STAGE[] = { 0, 2, 5 };
enum { START_COUNT = 3 };


static Settings s;
/* Command-line --cheat / --mute: this run only, never written to config.ini. */
static Settings s_cli;
static FILE *s_ram_trace;   /* --ram-trace: 2 KB of RAM appended per frame */

/* --poke: RAM writes held every frame. Cheats and RAM mapping both use this. */
typedef struct { uint16_t addr; uint8_t val; } Poke;
static Poke s_pokes[32];
static int  s_poke_count;

static void apply_pokes(const Poke *p, int n)
{
    for (int i = 0; i < n; i++) hw.ram[p[i].addr & 0x7FF] = p[i].val;
}

/* ---- settings ([Game] section of config.ini) ---- */

static void load_setting(void *ctx, const char *key, const char *value)
{
    (void)ctx;
    if (!strcmp(key, "StartMission")) { int v = atoi(value) - 1; s.start = (uint8_t)(v > 0 && v < START_COUNT ? v : 0); }
    for (int i = 0; i < CHEAT_COUNT; i++)
        if (!strcmp(key, CHEATS[i].key)) {
            if (atoi(value)) s.cheats |= (uint8_t)(1u << i);
            else s.cheats &= (uint8_t)~(1u << i);
        }
    for (int i = 0; i < CH_COUNT; i++)
        if (!strcmp(key, CH_KEYS[i])) {
            if (atoi(value)) s.mute |= (uint8_t)(1u << i);
            else s.mute &= (uint8_t)~(1u << i);
        }
}

static void save_settings(void *ctx, FILE *f)
{
    (void)ctx;
    for (int i = 0; i < CH_COUNT; i++)
        fprintf(f, "%s = %d\n", CH_KEYS[i], (s.mute >> i) & 1);
    for (int i = 0; i < CHEAT_COUNT; i++)
        fprintf(f, "%s = %d\n", CHEATS[i].key, (s.cheats >> i) & 1);
    fprintf(f, "StartMission = %d\n", s.start + 1);
}

/* ---- per frame ---- */

static bool s_reset_pending;
static long s_reset_at = -1, s_frames_begun;

static void frame_begin(void *ctx)
{
    (void)ctx;
    /* Reset is pressed here, just before the CPU runs: requested at frame end,
     * the host's post-frame work clears pending interrupts before it is taken. */
    if (s_reset_pending || ++s_frames_begun == s_reset_at) { s_reset_pending = false; cpu.do_reset = 1; }
    hw_apu_mute_mask = s.mute | s_cli.mute;
}

static uint8_t s_cheat_prev[CHEAT_COUNT][4];

/* $51 is 0 on the title and menus and 1 from take-off on, through deaths. */
#define IN_FLIGHT (hw.ram[0x051] == 1)

static void apply_cheats(void)
{
    /* Start at mission N: hold the stage until the first take-off, then let the
     * game run its own course. */
    uint8_t start = s_cli.start ? s_cli.start : s.start;
    if (start && !IN_FLIGHT) hw.ram[0x048] = START_STAGE[start];
    if (((s.cheats | s_cli.cheats) >> CHEAT_NOHIT & 1) && IN_FLIGHT && (hw.ram[0x440] & 0xF0) == 0xC0)
        hw.ram[0x440] = 0x40;
    for (int i = 0; i < CHEAT_COUNT; i++)
        for (int k = 0; k < CHEATS[i].slots; k++) {
            uint8_t *v = &hw.ram[CHEATS[i].addr + k], *prev = &s_cheat_prev[i][k];
            if (((s.cheats | s_cli.cheats) >> i & 1) && IN_FLIGHT && *prev != 0 && *v == (uint8_t)(*prev - 1))
                *v = *prev;
            *prev = *v;
        }
}

/* Skip to the next mission: set the stage to the next mission's first stage
 * and put the game in state 1, its mission intro ($50: 1 briefing -> 2,3,4
 * launch -> 5 flying). Verified: the next briefing is typed out. */
static bool s_skip_pending;
static long s_skip_at = -1, s_frame;

static void do_skip(void)
{
    if (!IN_FLIGHT || hw.ram[0x050] != 5) return;   /* only from normal flight */
    uint8_t stage = hw.ram[0x048], next = 0;
    for (int i = 0; i < START_COUNT; i++)
        if (START_STAGE[i] > stage) { next = START_STAGE[i]; break; }
    if (!next) return;                               /* already on the last mission */
    hw.ram[0x048] = next;
    hw.ram[0x440] = 0x40;
    hw.ram[0x050] = 1;   /* mission intro: briefing, then launch */
}

static void frame_end(void *ctx)
{
    (void)ctx;
    s_frame++;
    if (s_skip_pending || s_frame == s_skip_at) { s_skip_pending = false; do_skip(); }
    apply_cheats();
    apply_pokes(s_pokes, s_poke_count);
    if (s_ram_trace) fwrite(hw.ram, 1, sizeof hw.ram, s_ram_trace);
}

/* ---- developer options (also used by the headless checks) ---- */

static const CycHostOption OPTIONS[] = {
    { "--ram-trace", true, "append the 2 KB of CPU RAM to FILE after every frame (RAM mapping)" },
    { "--poke", true, "hold RAM bytes every frame: ADDR=VAL[,ADDR=VAL...] in hex, e.g. 12A=05" },
    { "--cheat", true, "turn on a cheat for this run: lives, missiles, nohit" },
    { "--reset-at", true, "press the console's Reset button at frame N (testing)" },
    { "--skip-at", true, "skip to the next mission at frame N (testing)" },
    { "--start-mission", true, "begin new games on mission 1, 2 or 3 (this run only)" },
    { "--mute", true, "silence APU channels: bitmask, 1=pulse1 2=pulse2 4=triangle 8=noise 16=DMC" },
};

static bool option(void *ctx, const char *name, const char *value)
{
    (void)ctx;
    if (!strcmp(name, "--ram-trace")) {
        s_ram_trace = fopen(value, "wb");
        return s_ram_trace != NULL;
    }
    if (!strcmp(name, "--poke")) {
        char buf[512];
        snprintf(buf, sizeof buf, "%s", value);
        for (char *tok = strtok(buf, ","); tok; tok = strtok(NULL, ",")) {
            unsigned a, v;
            if (s_poke_count == 32 || sscanf(tok, "%x=%x", &a, &v) != 2 || a > 0x7FF || v > 0xFF) return false;
            s_pokes[s_poke_count++] = (Poke){ (uint16_t)a, (uint8_t)v };
        }
        return true;
    }
    if (!strcmp(name, "--cheat")) {
        for (int i = 0; i < CHEAT_COUNT; i++)
            if (!strcmp(value, CHEATS[i].option)) { s_cli.cheats |= (uint8_t)(1u << i); return true; }
        return false;
    }
    if (!strcmp(name, "--reset-at")) {
        s_reset_at = strtol(value, NULL, 10);
        return s_reset_at > 0;
    }
    if (!strcmp(name, "--skip-at")) {
        s_skip_at = strtol(value, NULL, 10);
        return s_skip_at > 0;
    }
    if (!strcmp(name, "--start-mission")) {
        int v = atoi(value) - 1;
        if (v < 0 || v >= START_COUNT) return false;
        s_cli.start = (uint8_t)v;
        return true;
    }
    if (!strcmp(name, "--mute")) {
        char *end;
        long v = strtol(value, &end, 0);
        if (*end || v < 0 || v > 31) return false;
        s_cli.mute = (uint8_t)v;
        return true;
    }
    return false;
}

/* ---- menu rows ---- */

#ifdef CYC_WITH_RECOMP_UI
static const RecompRuntimeUiItem MENU[] = {
    { "tg2.mute.0", "Audio", "Mute pulse 1",  "Lead melody and some effects.", RECOMP_RUNTIME_UI_BOOL },
    { "tg2.mute.1", "Audio", "Mute pulse 2",  "Harmony and some effects.",     RECOMP_RUNTIME_UI_BOOL },
    { "tg2.mute.2", "Audio", "Mute triangle", "Bass line.",                    RECOMP_RUNTIME_UI_BOOL },
    { "tg2.mute.3", "Audio", "Mute noise",    "Drums, guns and explosions.",   RECOMP_RUNTIME_UI_BOOL },
    { "tg2.mute.4", "Audio", "Mute samples",  "Recorded sound clips (DMC).",   RECOMP_RUNTIME_UI_BOOL },
    { "tg2.reset", "System", "Reset game", "Like pressing Reset on the console: back to the title screen.",
      RECOMP_RUNTIME_UI_ACTION },
    { "tg2.start", "Cheats", "Start at mission", "New games begin on this mission.",
      RECOMP_RUNTIME_UI_CHOICE, 0, START_COUNT - 1, 1, START_NAMES, START_COUNT },
    { "tg2.skip", "Cheats", "Skip to next mission", "Jumps to the next mission's briefing. Works while flying.",
      RECOMP_RUNTIME_UI_ACTION },
    { "tg2.cheat.0", "Cheats", "Infinite lives",    "Losing a plane doesn't cost a life.",   RECOMP_RUNTIME_UI_BOOL },
    { "tg2.cheat.1", "Cheats", "Infinite missiles", "Firing doesn't use up missiles.",       RECOMP_RUNTIME_UI_BOOL },
    { "tg2.cheat.2", "Cheats", "Can't be hit", "Enemy missiles never hit your plane.",           RECOMP_RUNTIME_UI_BOOL },
};

/* "tg2.mute.N" -> &s.mute bit N, "tg2.cheat.N" -> &s.cheats bit N. */
static uint8_t *row_bits(const RecompRuntimeUiItem *item, int *bit)
{
    if (!strncmp(item->key, "tg2.mute.", 9))  { *bit = item->key[9] - '0';  return &s.mute; }
    if (!strncmp(item->key, "tg2.cheat.", 10)) { *bit = item->key[10] - '0'; return &s.cheats; }
    return NULL;
}

static int get_value(void *ctx, const RecompRuntimeUiItem *item, int *out)
{
    (void)ctx;
    if (!strcmp(item->key, "tg2.start")) { *out = s.start; return 1; }
    int bit;
    uint8_t *bits = row_bits(item, &bit);
    if (!bits) return 0;
    *out = (*bits >> bit) & 1;
    return 1;
}

static int set_value(void *ctx, const RecompRuntimeUiItem *item, int value)
{
    (void)ctx;
    if (!strcmp(item->key, "tg2.start")) { s.start = (uint8_t)(value >= 0 && value < START_COUNT ? value : 0); return 1; }
    int bit;
    uint8_t *bits = row_bits(item, &bit);
    if (!bits) return 0;
    if (value) *bits |= (uint8_t)(1u << bit);
    else *bits &= (uint8_t)~(1u << bit);
    return 1;
}

static int run_action(void *ctx, const RecompRuntimeUiItem *item)
{
    (void)ctx;
    if (!strcmp(item->key, "tg2.reset")) {
        s_reset_pending = true;
        return 1;
    }
    if (strcmp(item->key, "tg2.skip")) return 0;
    s_skip_pending = true;   /* applied at the end of the next frame */
    return 1;
}

static const RecompRuntimeUiCallbacks MENU_CB = { NULL, get_value, set_value, run_action };
#endif

/* The launcher's About page. Blank lines split paragraphs. */
static const char ABOUT_TEXT[] =
    "Hi, I'm Maddie. I make apps and games on my own at Tanooki Studios.\n\n"
    "I love old NES games, and I wanted to play them on a modern computer the way they "
    "deserve: crisp pixels at any size, a real settings screen, controllers that just work, "
    "and a few cheats for when a mission is being mean. So I rebuilt Top Gun: The Second "
    "Mission as a native program for Mac, Windows and Linux.\n\n"
    "It's free, and it'll stay free. You bring your own copy of the game; nothing from the "
    "original cartridge ships with this. It isn't made by or affiliated with Konami, "
    "Paramount or Nintendo.\n\n"
    "Making these is how I pay my bills, though. If this made you smile, a tip helps me "
    "keep going. No pressure at all.";

const CycHostExtras *cyc_host_extras(void)
{
    static const CycHostExtras extras = {
#ifdef CYC_WITH_RECOMP_UI
        .menu_items = MENU,
        .menu_item_count = sizeof MENU / sizeof MENU[0],
        .menu_callbacks = &MENU_CB,
#endif
        .load_setting = load_setting,
        .save_settings = save_settings,
        .frame_begin = frame_begin,
        .frame_end = frame_end,
        .options = OPTIONS,
        .option_count = sizeof OPTIONS / sizeof OPTIONS[0],
        .option = option,
        .about_text = ABOUT_TEXT,
        .about_image = "maddie-signature.png",   /* assets/signature.png, staged by CMakeLists.txt */
        .about_link_url = "https://tanookistudios.com/SupportMe",
        .about_link_label = "Leave a Tip",
    };
    return &extras;
}
