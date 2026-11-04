# Changelog

SparkPark is a fork of [PokePark Wiicompiled](https://github.com/MildRain479/PokePark-Wiicompiled) by MildRain479 (CC0). The game port itself is upstream's work: the static recompiler, the runtime and hardware replacement, the Vulkan and OpenGL renderers, audio, controls, minigame motion mappings, the in-game Options menu and the quality-of-life features.

This changelog lists what SparkPark adds on top. SparkPark never distributes game data or code built from it: its download is a builder that makes the game on the player's PC from their own disc.

## Unreleased

Roadmap Batch 1: profiling, Vulkan, and quality-of-life settings. Measurements are in `docs/PERFORMANCE.md`.

### Added
- **Profiling for developers:**
  - `docs/PERFORMANCE.md`: how to measure (upstream's `RECOMP_FPS` and `RECOMP_PROFILE`), the baseline, and before/after numbers for each change.
  - `SPARKPARK_DEBUG_INFO=1` gives the player-path build symbols for the profiler.
  - The profiler's split now counts the indirect-call lookup, the paired-single helpers and the FIFO decoder's loops in their kind.
- **Vulkan in the builder's runtime.** `build_player.py runtime` builds upstream's Vulkan renderer in when the Vulkan SDK is present. It compiles SparkPark's shaders to SPIR-V on the developer side; players need no SDK.
  - In the park it cuts the CPU's work per frame from about 24 ms to about 7 ms, and holds 60 fps where OpenGL managed about 36.
  - `builder/package.py stage` refuses an OpenGL-only runtime unless `--opengl-only` is given.
  - `NOTICES.txt` lists the Vulkan headers (MIT).
- **Settings** (Options and `settings.ini`; off means upstream's behaviour):
  - **Widescreen** (on): the game's own 16:9 mode, or 4:3; from the next start. `RECOMP_WIDESCREEN` still overrides it.
  - **Pause in background** (on): while another window is in front, the game pauses with its clock stopped, its sound held, rumble off and no input taken.
  - **Show FPS** (off, F3): a frame rate counter in the top-left corner.
  - **Hold + to skip movies** (on): holding + skips a movie such as the 56 s opening, with a "Skipping" bar while held.
- **F12 screenshots,** saved as PNG in `Pictures\SparkPark` without the counter or the skip bar.

### Changed
- The game clock's pause (`ppc_timebase_pause`) now works on Windows too, not only Android.

## v0.2.0-alpha — 2026-11-04

The first SparkPark release players can use. Its one download, `SparkPark-v0.2.0-alpha.zip`, is a **one-time builder**: it contains no game code and nothing from the disc. The game itself is still upstream's port; see the top of this file.

### Added
- **The one-time builder** (`SparkPark.exe`, `builder/`). It is the design in `docs/launcher/sparkpark-launcher.html`, shown through WebView2, with its one button wired to real steps:
  - **Upload ISO → Checking disc… (with progress).** The Redump check from v0.1.0, with the reason shown on the sign if it fails.
  - **Build Recomp → Building… N%.** It extracts the disc (dtk), recompiles the game code (upstream's recompiler, on embedded Python), and compiles and links it with the bundled llvm-mingw against a prebuilt runtime library. A failed build shows the reason and **Retry Build**.
  - **Play.** Every later start goes straight into the game. Hold Shift or use `--rebuild` to see the builder again.
  - It works with the mouse, Enter/Space or a controller's A button.
  - Data is kept in `%LOCALAPPDATA%\SparkPark`, or beside the builder with `portable.txt`.
  - The Baloo 2 font is bundled (SIL OFL), with nothing loaded from the internet.
- **Building without Visual Studio** (`tools/sparkpark/build_player.py`). The runtime is prebuilt as a library from a header made from the public function list alone, so it holds no game code. The player's PC compiles only the code recompiled from their own disc.
- **`builder/audit.py`.** It proves a release zip holds no game code or disc data (see `docs/PROGRESS.md`, phase 3).
- `README.txt`, `NOTICES.txt` and a `licenses` folder in the zip, covering every bundled component.

### Changed
- `boot.c` includes `<mmsystem.h>`, needed by mingw-w64 for `timeBeginPeriod`.
- The README describes the builder download.

## v0.1.0-alpha — 2026-11-03

First SparkPark pre-release, **source only**: build it yourself from your own disc. Based on upstream at [`f98b708`](https://github.com/MildRain479/PokePark-Wiicompiled/commit/f98b708669abac389f943de288ef2190bc03c2f7) ("An Android app, and Vulkan made faster everywhere").

### Added
- **Full disc verification.** On first run, before anything is extracted, the disc image is checked against the Redump dump of PokéPark Wii (USA), R8AE01 Rev 0 (CRC32 `26e533d5`, SHA-1 `c9a6f7601fd54b5e21a0909084839d30874a9a37`).
  - Upstream checked only the 6-byte game ID.
  - A failed check says why: not a disc image, another game, the wrong region (Europe, Japan, Korea, Taiwan), the wrong revision, an NKit/scrubbed/trimmed image, or a modified one.
  - It works for every format decomp-toolkit reads (ISO, RVZ, WIA, WBFS, CISO…). See `recomp/boot/disc_verify.c`.
  - 13 unit checks in `recomp/tests/test_disc_verify.cpp`.
- **A guard in `build.py`.** `--public` assembles a release folder whose exe contains game code recompiled from your disc. It now refuses and explains why, unless `--public-local-only` is given for local testing. `--dist` and `--android` remind you not to share what they build.
- **Docs:**
  - `docs/ARCHITECTURE.md`: how the port reads the disc, where the recompiled code lives, the runtime by area, the build pipeline, and where SparkPark's builder and pointer work hook in.
  - `docs/BUILDER-PLAN.md`: research for building on the player's PC without Visual Studio (toolchains, licenses, sizes, build time, options).
  - `docs/PROGRESS.md`: the development log.
  - `CLAUDE.md`: the project brief.

### Changed
- **Rebrand to SparkPark** (Windows side): the program is `SparkPark.exe` (`build.py --dist`); the window and frame-rate titles, the Vulkan application name, message boxes, the first-run window and the `settings.ini` header say SparkPark.
- **README** rewritten for SparkPark:
  - an unofficial PC recompilation, credited to upstream;
  - bring your own disc (R8AE01 Rev 0, verified by hash);
  - building on Windows with Visual Studio 2022 or 2026 (OpenGL works without the Vulkan SDK);
  - known issues;
  - a built program must never be shared.

  Now clean UTF-8.
- `.gitignore` gains SparkPark's rules for disc images, extracted files, saves and dumps, appended after upstream's.

### Unchanged on purpose
- Upstream's license (CC0 1.0), credits and third-party notices (decomp-toolkit, nod, the PokePark Wii decompilation, Icons8).
- Upstream's Android code. SparkPark is developed and tested on Windows only for now.
