# Top Gun: The Second Mission (USA) - RAM map

Found with `--ram-trace` + `tools/ram_find.py`, and `--trace-frame/--trace-out` +
`tools/vram_writes.py`. An entry is **confirmed** only after a `--poke` changed
what the screen shows.

## Confirmed

| Addr | Meaning | Proof |
|------|---------|-------|
| $0068 | Missiles left for the selected type (binary, 60 = 0x3C) | `--poke 68=05` -> HUD shows 05 (route_missiles, frame 3700) |
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
