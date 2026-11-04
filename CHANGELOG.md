# Changelog

SparkPark is a fork of [PokePark Wiicompiled](https://github.com/MildRain479/PokePark-Wiicompiled) by MildRain479 (CC0). The game port itself is upstream's work: the static recompiler, the runtime and hardware replacement, the Vulkan and OpenGL renderers, audio, controls, minigame motion mappings, the in-game Options menu and the quality-of-life features.

This changelog lists what SparkPark adds on top. SparkPark is source-only and never distributes game data or code built from it.

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
