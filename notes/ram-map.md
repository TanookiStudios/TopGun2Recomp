# Top Gun: The Second Mission (USA) - RAM map

Found with `--ram-trace` + `tools/ram_find.py`, and `--trace-frame/--trace-out` +
`tools/vram_writes.py`. An entry is **confirmed** only after a `--poke` changed
what the screen shows.

## Confirmed

| Addr | Meaning | Proof |
|------|---------|-------|
| $0045 | Lives left. Drops 3,2,1,0 on each loss; game over after 0 | `--cheat lives`: no drops, still playing at frame 13500 (game over ~9500 without) |
| $0051 | 0 on title/menus, 1 from take-off on (stays 1 through deaths) | gate for cheats; holding $45 from power-on froze the title |
| $0057 | Selected missile type (index) | firing code LDY $57 / LDA $0068,Y / SBC #1 / STA $0068,Y (PRG bank 10) |
| $0068-$006A | Missiles left per type ($68+type). Mission 1: 60, 60, 0 | `--poke 68=05` -> HUD shows 05. Infinite-missile cheat not yet seen fire: scripted runs never lock a target |
| $0048 | Stage. Briefings at stages 0, 2, 5 = missions 1, 2, 3; stages between are later legs | `--poke 48=02` -> briefing types "MISSION #2"; text ID = $48 + $34 (code at $A770, PRG bank 3) |
| $0050 | Game state: 1 mission intro/briefing, 2-4 launch, 5 flying, 6 hit, 11-15 crash/reload | skip: $48 = next mission start, $50 = 1 -> next briefing, then 2,3,4,5 |
| $0052 | Event code, set and consumed in one frame: 2 = player destroyed ($8BB9 -> $F452) | trace of frame 5319 |
| $0440 | Player plane: 0x40 flying, 0xE1 enemy missile closing, 0xCx hit (C8, C2 seen) | can't-be-hit cheat resets 0xCx -> 0x40; 30000-frame idle run, no losses |
| $078E | Missile count last drawn on the HUD; code at $A765 redraws when $68 != $78E | trace of frame 3677 |

## How the HUD is drawn

- `$0300` is the PPU update queue: `$0300` length, `$0301/$0302` VRAM address
  lo/hi, data from `$0303`, `$FF` terminator. NMI code at $E019-$E04C drains it.
- HUD digits are tiles = digit + 3 (tile 03 = "0").
- Speed digits: VRAM $2E97-$2E9A. Missile digits: VRAM $2E9C-$2E9D.
- Binary -> two digits via table lookup at $A9EB.

## Observed, unconfirmed

| Addr | Notes |
|------|-------|
| $012A | Holds 40 through mission 1; poking it does nothing visible. |
| $0072 | Counts 4..9 during flight around frames 3372-3508 of route_missiles. |

## Dead ends
- Searching RAM for HUD numbers as BCD or digit bytes fails: digits are tile = digit + 3,
  and the source is a binary value.
- $006B looks like fuel at a glance (250 -> 200) but changes every ~8 frames to random values: an RNG, not fuel.

## Widescreen (topgun2_extras.c compose)
- In flight ($51 == 1) sky/sea rows extend with each row's most common color; rows the
  split scrolls into the lower nametables (line scroll Y >= 240) are cockpit -> dark bezel.
- Verified: 16:9 = 426 px, 21:9 = 560 px; title stays pillarboxed (black wings);
  6000-frame --hash-out identical with and without widescreen (presentation only).
- Known limit: enemies only exist on screen inside the native 256 columns; the wings
  show sky, never planes.
