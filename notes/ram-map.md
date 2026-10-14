# Top Gun: The Second Mission (USA) - RAM map (work in progress)

Found with `--ram-trace` runs + `tools/ram_find.py`. Each entry needs a poke check
(`--poke ADDR=VAL`, then a screenshot) before a cheat may use it.

| Addr | Status | Notes |
|------|--------|-------|
| $12A | unconfirmed | Holds 0x28 (40) all through mission 1 in route_mission1; poking 05 did not change the HUD "40" (HUD may redraw only on fire). Not the HUD source when it read 60. |
| $072 | observed | Counts up 4..9 during flight around frames 3372-3508 of route_missiles (scene/phase counter?). |

Dead ends (don't repeat):
- Speed "0830" is not stored as BCD, 16-bit LE, or 0/8/3/0 digit bytes.
- No byte falls steadily during flight, so fuel (if any) isn't a simple countdown.
- Holding A (route_missiles) never changed $12A.
