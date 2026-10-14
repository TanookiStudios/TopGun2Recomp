/*
 * topgun2_extras.c - Top Gun: The Second Mission's additions to the NESRecomp
 * cycle host (runner/cyc/cyc_host_extras.h): its own menu rows, its own
 * config.ini [Game] values, and per-frame work.
 *
 * Contains no ROM bytes. Everything here is applied to the running machine.
 */
#include "cyc_host_extras.h"
#include "hw_internal.h"   /* hw.ram: the 2 KB of CPU work RAM */

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

typedef struct {
    uint8_t mute;   /* bit per channel, CH_KEYS order */
} Settings;

static Settings s;
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
}

/* ---- per frame ---- */

static void frame_begin(void *ctx)
{
    (void)ctx;
    hw_apu_mute_mask = s.mute;
}

static void frame_end(void *ctx)
{
    (void)ctx;
    apply_pokes(s_pokes, s_poke_count);
    if (s_ram_trace) fwrite(hw.ram, 1, sizeof hw.ram, s_ram_trace);
}

/* ---- developer options (also used by the headless checks) ---- */

static const CycHostOption OPTIONS[] = {
    { "--ram-trace", true, "append the 2 KB of CPU RAM to FILE after every frame (RAM mapping)" },
    { "--poke", true, "hold RAM bytes every frame: ADDR=VAL[,ADDR=VAL...] in hex, e.g. 12A=05" },
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
    if (!strcmp(name, "--mute")) {
        char *end;
        long v = strtol(value, &end, 0);
        if (*end || v < 0 || v > 31) return false;
        s.mute = (uint8_t)v;
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
};

static int mute_index(const RecompRuntimeUiItem *item)
{
    return strncmp(item->key, "tg2.mute.", 9) ? -1 : item->key[9] - '0';
}

static int get_value(void *ctx, const RecompRuntimeUiItem *item, int *out)
{
    (void)ctx;
    int i = mute_index(item);
    if (i < 0) return 0;
    *out = (s.mute >> i) & 1;
    return 1;
}

static int set_value(void *ctx, const RecompRuntimeUiItem *item, int value)
{
    (void)ctx;
    int i = mute_index(item);
    if (i < 0) return 0;
    if (value) s.mute |= (uint8_t)(1u << i);
    else s.mute &= (uint8_t)~(1u << i);
    return 1;
}

static const RecompRuntimeUiCallbacks MENU_CB = { NULL, get_value, set_value };
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
