# SparkPark progress log

## 2026-11-03: Fork session 1, Build Order steps 1-2 (fork, build as-is, map the code)

**Before this session.** SparkPark started as its own recomp on NWiiRecomp. That work reached the game's strap screen and is archived at `D:\SparkPark-nwii` (git history intact). From now on SparkPark is a fork of [MildRain479/PokePark-Wiicompiled](https://github.com/MildRain479/PokePark-Wiicompiled) (CC0).

**Done**
- Cloned `Dvrkie13/SparkPark` to `D:\SparkPark`, with `upstream` = `MildRain479/PokePark-Wiicompiled`. Both are at `f98b708` ("An Android app, and Vulkan made faster everywhere").
- Toolchain:
  - Python 3.13 works.
  - VS 2026 Build Tools (MSVC 14.51) work: `build.py` finds them through `vswhere -latest`, so VS 2022 isn't needed.
  - No Vulkan SDK, so this build is OpenGL only. Installing it was not approved.
  - dtk **v1.6.2** (the version upstream asks for) is at `build/tools/dtk.exe`; its sha256 `7143e8d6…0c0444` matches the GitHub release digest.
- Game data stays out of the repo folder. `orig/` and `build/` are directory junctions:
  - `orig` → `D:\SparkPark-data\upstream\orig`
  - `build` → `D:\SparkPark-build\upstream`
  - Both are also git-ignored.
- Extracted the verified R8AE01 Rev 0 ISO with dtk 1.6.2: 373 files, `main.dol` sha1 `8c241f2a…`.
- `recomp.py` translated 14,437 functions with 0 untranslated instructions (17 s). `build.py --run test` passed 2,814,740 checks with 0 failures (1.7 min).
- **Run results:**
  - It reaches the title screen ("Please Press 2", movie background) about 16 s after start, at a steady ~60 fps (OpenGL, 2x).
  - A scripted run (`RECOMP_PAD_SCRIPT` pressing 2, movies skipped) went through picture choice, "New game created", the main menu and the Mew intro into the park.
- Wrote [ARCHITECTURE.md](ARCHITECTURE.md).
- Merged SparkPark's ignore rules into upstream's `.gitignore`, appended after theirs. No tracked file is affected.
- Added `CLAUDE.md` and the builder mockups (`docs/builder-1…5.png`).

**Notes**
- **README gap.** Upstream's README says to extract into `orig\R8AE01\disc`, but `boot.exe` derives the disc root from the DOL's folder. So a development run also needs `orig\R8AE01\sys\main.dol` (copied from `disc\sys\`). Worth a small upstream README PR.
- **Pointer support.** CLAUDE.md says upstream has no pointer support. In fact `hle_wpad.c`/`hle_motion.c` feed IR dots from the mouse or right stick, but only in Rayquaza's and Rotom's minigames. A general pointer (menus, photo mode) is what's missing.
- **SD card.** CLAUDE.md's runtime list includes the SD card. Upstream refuses `/dev/sdio`, so the game sees no card. In-game pictures live in the NAND save (`Pic0-3`).
- **Disc verification.** CLAUDE.md wants Redump-hash verification. Upstream's first-run import checks only the game ID (`R8AE01`), not the revision or hash.
- **Builder model** (decision needed before step 4). CLAUDE.md's builder recompiles and links on the player's PC. Upstream compiles on the developer's PC and ships an exe that already contains the translated code; the player only extracts the disc. See ARCHITECTURE.md §5.
- **Right stick.** In the park the right stick is free (only its click is bound, to HOME), and the camera is mouse-only. Pointer vs camera on the right stick is a design choice for step 5.

**Next (step 3, Rebrand, next session)**
- Rename the exe, window title, Android app name and README branding to SparkPark, keeping upstream credit and licenses.
- Decide on the open questions above: builder model, Android yes/no, right stick pointer vs camera.
