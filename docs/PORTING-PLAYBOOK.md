# Tanooki Studios NES port playbook

How every Tanooki Studios NES port is made, so they all look and work the same from start to
finish. **Top Gun 2 (this repo) is the reference port.** Copy from it; don't reinvent anything
listed here. Where this file and the reference disagree, the reference repo wins, then fix this file.

Reference repos (all on the **TanookiStudios** GitHub account):
- Game: `TanookiStudios/TopGun2Recomp` (local: `~/Documents/GitHub/TopGun2Recomp`)
- Website: `TanookiStudios/TopGun2-Site` (local: `~/Documents/GitHub/TopGun2-Site`, private)

---

## 0. Rules that never change

- **Maddie writes no code and doesn't know the jargon.** Plain-English updates, American English.
- **No ROM and nothing made from it in any repo.** ROM lives in `rom/` (git-ignored). Generated C stays
  in build folders. Before the first push, scan tracked files for ROM byte samples (see section 7).
- **Original art only.** No console-maker logos, no box art, no film/game logos. Icons and site art
  are drawn by us.
- **GitHub = TanookiStudios only.** In every new repo, before the first commit:
  `git config user.name Maddie` and
  `git config user.email 337555733+TanookiStudios@users.noreply.github.com`.
  Before every push: every commit's email is that address, and `origin` is `github.com/TanookiStudios/...`.
  (Her personal email is verified on an old account; commits signed with it show the old account.)
- **Finished builds go to `~/Downloads`** (the build scripts already do it).
- **Pages for Maddie to look at or click open in the Claude tab group in her browser (Brave)**, not the
  app's built-in preview pane.
- Free forever; NESRecomp is PolyForm Noncommercial, so ports can never be sold.

## 1. Before starting: can NESRecomp do this game?

Read the ROM's iNES header (byte 6 high nibble + byte 7 high nibble = mapper). NESRecomp supports
mappers 0 (NROM), 1 (MMC1), 4 (MMC3), 66 (GxROM) and others listed in `vendor/nesrecomp/runner/cyc/MAPPERS.md`.
If the mapper isn't supported, say so before doing anything else.

## 2. New repo from the template

Copy the Top Gun 2 repo minus `vendor/ build*/ work/ dist/ rom/ .git`, then rename:

| Thing | Top Gun 2 value | Change to |
|---|---|---|
| Repo / folder | `TopGun2Recomp` | `<Game>Recomp` |
| CMake target | `TopGun2` | short game name |
| `game.toml` `output_prefix`, `name` | `topgun2`, `Top Gun: The Second Mission` | the game |
| ROM file | `rom/topgun2.nes` | `rom/<short>.nes` |
| Game code file | `topgun2_extras.c` | `<short>_extras.c` (keep the structure) |
| App name / bundle id | `Top Gun 2`, `com.tanookistudios.topgun2recomp` | the game |
| Settings folder (Mac) | `~/Library/Application Support/TopGun2` | follows the program name automatically |
| `PLAYERS` in CMakeLists | `1` | the game's player count |

Keep unchanged: `setup.sh` (pinned engine versions), every file in `patches/`, `cmake/`, `tools/`,
`assets/signature*.svg`, `assets/controllers/`, `LICENSE`, `THIRD_PARTY_NOTICES.md`.

### The engine patches (all ports get all of them)

| Patch | What it gives the player |
|---|---|
| 0001 | input scripts wait on real frames (testing) |
| 0002 | Mac settings in Application Support (+ one-time move); per-channel sound mute |
| 0003 | "Fit" width keeps sharp whole-number zoom |
| 0004 | window lays the picture out itself (no off-center/cropped picture after resizes), menu row "Close game", controller database + generic-pad layout, test hooks |
| 0005 | Reset works (reset beats a pending NMI) |
| 0006 | launcher **Game** page and **About** page plumbing |
| 0007 | one-player games: player 1 = keyboard **and** first controller |
| recomp-ui-0001 | "Smooth pixels (blurry)" label; Game and About pages in the launcher |

## 3. House style for the program

- **No widescreen.** Maddie removed it on purpose. Don't add it back.
- **Crisp scaling:** integer scale on, smoothing off by default.
- **Launcher** (shows before play): buttons **Game · Settings · About**, header mark = the port's own
  icon (CMakeLists copies `assets/icon/<short>-256.png` over `brand_nes.tga`).
  - **Game page:** the port's own options, in sections **Audio** (five channel mutes, same wording as
    Top Gun 2) and **Cheats**.
  - **About page:** Maddie's story (template below), signature image, **Leave a Tip** button →
    `https://tanookistudios.com/SupportMe`.
- **Esc menu:** Display, Graphics, Audio, System (Resume, Save/Load state, **Reset game**, **Close game**),
  Controls, Cheats. Right shoulder button opens it on a controller.
- **Controllers:** must work with no setup (patch 0007 + bundled `gamecontrollerdb.txt`). Prove it with
  the TCP checks in section 6.
- **Cheats, the standard set** where the game allows: infinite lives, infinite ammo/missiles/items,
  can't be hit, start at any level/mission, skip to next level. Pattern: undo a drop of exactly one,
  only while an "in play" flag is set (never hold values from power-on: it can freeze title screens).

### About text template

Paragraph 1: "Hi, I'm Maddie. I make apps and games on my own at Tanooki Studios."
Paragraph 2: why this game, one or two sentences, in her voice; no invented memories.
Paragraph 3: "It's free, and it'll stay free. You bring your own copy of the game; nothing from the
original cartridge ships with this. It isn't made by or affiliated with <publisher>, <other owners> or Nintendo."
Paragraph 4: "Making apps (including some decomps) is how I pay my bills. If this game brought you
some nostalgia and <a funny line about this game's hardest moment>, feel free to drop me a tiny tip.
It really helps!"

## 4. The icon

Original pixel art with `tools/make_icon.swift`: a 32×32 grid, horizontal sky/scene bands in NES-style
colors, one hero sprite from the game's *theme* drawn by us (never the game's own sprite), rounded
square. Same file renders the macOS layout (`icon-1024.png` → `AppIcon.icns`) and the full-bleed
Windows set (`<short>.ico`, 7 sizes). Show Maddie the 1024 render before moving on.

## 5. Making it run (the method that worked)

1. `./setup.sh`, put the ROM in `rom/`, configure `build-headless` (HEADLESS) and `build`.
2. Headless boot with `--screenshot ... --shot-every 300`; look at the frames.
3. Coverage: `--miss-log seeds.txt` on scripted routes (`tests/route_*.txt`), rebuild, repeat to ~100% native.
4. Correctness: same route with `--hash-out` native vs `--interp-only` → identical.
5. Every later change: re-run the 6000-frame route hash against the first recording (proves gameplay unchanged).

## 6. Mapping RAM for cheats (proof before use)

- `--ram-trace FILE` (2 KB per frame) + `tools/ram_find.py` to find candidates.
- A value counts as found only when `--poke ADDR=VAL` visibly changes the screen.
- For what the HUD shows: `--trace-frame N --trace-out` + `tools/vram_writes.py`, follow the write back
  to the RAM byte (HUD digits are often tiles = digit + offset, not plain numbers).
- For a game event (hit, level change): find the state byte that changes at the event, trace that frame.
- Record everything in `notes/ram-map.md` with its proof; keep dead ends too.
- Controller proof: hidden window `--hidden --tcp 4399`, TCP `state` → `nes` must show the right bits
  for keyboard (`key` with `name`), a standard pad and `NESRECOMP_VPAD_GENERIC=1`.

## 7. Release

1. Version tag `vX.Y.Z`.
2. `tools/make_app.sh --notarize` (universal Mac app, Developer ID, notarized; needs the screen unlocked),
   `tools/make_windows_zip.sh` (cross-built here), `tools/make_linux.sh` (Ubuntu 20.04 in Colima:
   `colima start --vm-type vz --vz-rosetta --arch aarch64`; `colima stop` after).
3. `tools/capture-screenshots.sh` (unattended; launcher shows ~5 s; warn Maddie first). 12 shots:
   gameplay across levels, launcher Main/Game/Settings/About, Esc menu.
4. ROM-safety scan of tracked files; `.gitignore` blocks `*.png` except `assets/icon/`, `assets/signature.png`,
   `docs/screenshots/`.
5. README in the Top Gun 2 shape: icon, title, website link, **download table with
   `/releases/latest/download/<file>` links**, how to play, ROM SHA-256, controls, features, screenshots,
   build steps, About Maddie + signature + tip, credits/legal.
6. Push (checks from section 0), `gh release create` with the three files, then download each through
   the README links and `cmp` against `dist/`. Open the repo page in the tab group and confirm no
   image is broken (GitHub lazy-loads; scroll before checking).

## 8. Website (same theme every time)

Copy `TopGun2-Site`: big CSS-drawn gray cartridge with ridges, dark label with the port's icon art over
a band background, title in Press Start 2P, red NES-style download buttons, CSS-drawn two-button
controller in Controls, cream "front plate" panels, screenshots grid, About Maddie, legal footer.
Change only: label colors/art, title, text, screenshots, download links.

- Page `<title>`: "`<Game Title>` - Native For Windows, Mac, and Linux". Top-bar name: the full game title.
- Cloudflare Pages project via the pinned wrangler in `TanookiStudios-Site/node_modules`; subdomain
  `<name>.tanookistudios.com`. Attach the domain by API (token in `~/.wrangler/config/default.toml`,
  refresh by running any wrangler command first). The CNAME record needs Maddie's yes; add it in her
  Cloudflare dashboard in the Brave tab group (type CNAME, name `<sub>`, target `<project>.pages.dev`,
  proxied). Verify the address loads in her browser.

## 9. Announcement

Reddit post in the Top Gun 2 shape: what it is (static recompilation, not an emulator), links to site
and GitHub, features list, how to play with your own ROM, credits to NESRecomp, legal line, the tip
paragraph. Suggest r/nes, r/retrogaming, r/emulation, r/macgaming, r/linux_gaming, one per day; never link a ROM.

## 10. SNES games

Same house style, different engine: **snesrecomp** (`github.com/mstan/snesrecomp`, same author and
community as NESRecomp; existing ports include Super Mario World, Super Metroid, A Link to the Past,
Star Fox, Mega Man X 1-3). The launcher and menus come from the same recomp-ui, so the Game and About
pages (recomp-ui-0001) should carry over; check the pinned recomp-ui version first.

Not carried over automatically: everything in the NES patches (0001-0007) lives in NESRecomp's runner.
For an SNES port, check snesrecomp for each player-facing fix in the patch table (Mac settings folder,
per-channel mute, crisp fit, Reset, one-player keyboard + controller, controller database, Close game)
and port the missing ones to snesrecomp as `patches/` the same way. Look at an existing snesrecomp
game repo (e.g. SuperMarioWorldRecomp) for its CMake/config shape before copying ours.

**Website:** use `TopGun2-Site/templates/snes-site/` (same theme; a 16-bit-era cartridge with side
grips and a four-button controller with L/R, both drawn in CSS). Fill every `{{...}}`, add the
screenshots grid, set the SNES controls rows (Y/X/L/R are already in the table).
