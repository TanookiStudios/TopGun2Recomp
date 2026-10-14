/*
 * topgun2_extras.c - Top Gun: The Second Mission's additions to the NESRecomp
 * cycle host (runner/cyc/cyc_host_extras.h): its own menu rows, its own
 * config.ini [Game] values, and per-frame work.
 *
 * Contains no ROM bytes. Everything here is applied to the running machine.
 */
#include "cyc_host_extras.h"
#include "hw_internal.h"   /* hw.ram: the 2 KB of CPU work RAM */
#include "cyc_render.h"
#include "cyc_video.h"

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
    uint8_t wide;     /* WIDE_NAMES index: 0 off .. 4 fit the window */
    uint8_t start;    /* START_NAMES index: which mission a new game begins on */
} Settings;

/* $48 is the stage. Briefings appear at stages 0, 2 and 5 (missions 1-3);
 * the stages between are later legs of the same mission (notes/ram-map.md). */
static const char *const START_NAMES[] = { "Mission 1", "Mission 2", "Mission 3" };
static const uint8_t START_STAGE[] = { 0, 2, 5 };
enum { START_COUNT = 3 };

/* Widescreen choices, in NES_VIDEO_* order (common/nes_video_geometry.h):
 * stock 256, 16:9 426, 21:9 560, 32:9 854, fit = the window's own shape. */
static const char *const WIDE_NAMES[] = { "Off", "16:9", "21:9", "32:9", "Fit my screen" };
enum { WIDE_COUNT = 5 };

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

static void apply_wide(int mode);

static void load_setting(void *ctx, const char *key, const char *value)
{
    (void)ctx;
    if (!strcmp(key, "Widescreen")) apply_wide(atoi(value));
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
    fprintf(f, "# 0 off, 1 16:9, 2 21:9, 3 32:9, 4 fit the window\nWidescreen = %d\n", s.wide);
    fprintf(f, "StartMission = %d\n", s.start + 1);
}

/* ---- per frame ---- */

static void frame_begin(void *ctx)
{
    (void)ctx;
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

/* ---- widescreen ----
 *
 * The NES draws 256 columns; the host gives us a wider canvas. In flight the
 * view above the cockpit is horizontal sky/sea bands, so each row's wings take
 * that row's most common color from just inside the native picture (a sprite
 * at the edge can't streak across). Cockpit rows - the ones the mid-frame
 * split scrolls into the lower nametables - get a dark bezel. Anything not in
 * flight (title, menus, briefing art) stays pillarboxed. */

#define BEZEL 0xFF101018u

/* A row's most common color across columns 8..247 (past the NES left-column
 * mask). Sky/sea bands span the whole row, so this outvotes any sprite. */
static uint32_t row_band_color(const uint32_t *row)
{
    uint32_t best = row[8];
    int best_n = 0;
    for (int i = 8; i < 248; i += 16) {
        int n = 0;
        for (int j = 8; j < 248; j += 2) n += row[j] == row[i];
        if (n > best_n) best_n = n, best = row[i];
    }
    return best;
}

static int compose(uint32_t *out, int width, int height, int x0, const uint32_t *native, void *user)
{
    (void)user;
    if (!IN_FLIGHT) return 0;
    for (int y = 0; y < height; y++) {
        const uint32_t *src = native + y * 256;
        uint32_t *dst = out + y * width;
        bool cockpit = cyc_render_line_scroll_y(y) >= 240;
        uint32_t band = cockpit ? BEZEL : row_band_color(src);
        for (int x = 0; x < x0; x++) dst[x] = band;
        memcpy(dst + x0, src, 256 * sizeof *src);
        /* The game hides the leftmost 8 columns (PPUMASK); in the sky that
         * would be a black stripe between the wing and the picture. */
        if (!cockpit) for (int x = 0; x < 8; x++) dst[x0 + x] = band;
        for (int x = x0 + 256; x < width; x++) dst[x] = band;
    }
    return 1;
}

static void apply_wide(int mode)
{
    if (mode < 0 || mode >= WIDE_COUNT) mode = 0;
    s.wide = (uint8_t)mode;
    cyc_render_set_compositor(compose, NULL);
    cyc_video_set_mode(NES_VIDEO_STOCK + mode);
}

/* ---- developer options (also used by the headless checks) ---- */

static const CycHostOption OPTIONS[] = {
    { "--ram-trace", true, "append the 2 KB of CPU RAM to FILE after every frame (RAM mapping)" },
    { "--poke", true, "hold RAM bytes every frame: ADDR=VAL[,ADDR=VAL...] in hex, e.g. 12A=05" },
    { "--cheat", true, "turn on a cheat for this run: lives, missiles, nohit" },
    { "--widescreen", true, "presented width: off, 16:9, 21:9, 32:9, fit" },
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
    if (!strcmp(name, "--widescreen")) {
        static const char *const names[] = { "off", "16:9", "21:9", "32:9", "fit" };
        for (int i = 0; i < WIDE_COUNT; i++)
            if (!strcmp(value, names[i])) { apply_wide(i); return true; }
        return false;
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
    { "tg2.wide", "Graphics", "Widescreen",
      "Fills wide and ultrawide screens in flight. Fit my screen matches any monitor, up to 32:9.",
      RECOMP_RUNTIME_UI_CHOICE, 0, WIDE_COUNT - 1, 1, WIDE_NAMES, WIDE_COUNT },
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
    if (!strcmp(item->key, "tg2.wide")) { *out = s.wide; return 1; }
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
    if (!strcmp(item->key, "tg2.wide")) { apply_wide(value); return 1; }
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
    if (strcmp(item->key, "tg2.skip")) return 0;
    s_skip_pending = true;   /* applied at the end of the next frame */
    return 1;
}

static const RecompRuntimeUiCallbacks MENU_CB = { NULL, get_value, set_value, run_action };
#endif

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
    };
    return &extras;
}
