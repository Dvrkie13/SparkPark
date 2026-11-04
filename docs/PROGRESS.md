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

## 2026-11-03: Fork session 2, Build Order step 3 (rebrand) + disc verification + builder research

**Done**
- Committed the owner's updated CLAUDE.md: distribution model (build on the player's PC), right stick camera/pointer split, Android left untouched.
- **Rebrand (Windows side; Android untouched):**
  - `build.py --dist`/`--public` now write `SparkPark.exe`.
  - Window title `SparkPark` (`window_win32.c`); fps title `SparkPark - … fps` (`gx_frames.c`); Vulkan application name; `boot.c` and `import_win32.c` message box/window titles and texts; the `settings.ini` header.
  - `recomp/assets/release_readme.txt` and `README.md`. README covers SparkPark, an unofficial PC recompilation, forked from MildRain479's PokePark Wiicompiled with credit, bring your own disc, no game data, and that built exes must not be shared. It's now clean UTF-8 (upstream's had one stray cp1252 byte).
  - Left alone: comments that describe the game, internal window-class names, the Android app, and upstream's LICENSE and credits (kept, with the fork credit added on top).
- **Disc verification** (`recomp/boot/disc_verify.c/.h`, new). Before extracting, the first-run import now runs:
  - `dtk disc info`: game ID / revision / lossless;
  - `dtk disc verify`: CRC32 + SHA-1 of the whole disc;
  - a comparison with the Redump values for R8AE01 Rev 0 (`26e533d5`, `c9a6f760…874a9a37`, 4,699,979,776 bytes).

  Failures name the cause: unreadable, another game, wrong region (PAL/JP/KR/TW), wrong revision, NKit/scrubbed (NKit magic, a lossy image, or a short ISO), or modified. The dtk output is captured through a pipe while the window keeps responding.
  - 13 unit checks in `recomp/tests/test_disc_verify.cpp` (in the `test` target), all passing.
  - Also checked on real dtk output: the owner's ISO → OK; `main.dol` as an "image" → unreadable; Rev 1 header → wrong revision; one changed SHA-1 digit → modified.
  - *Not yet tried:* clicking through the actual import window, which needs a manual run from a clean folder.
- **docs/BUILDER-PLAN.md**: toolchain options (llvm-mingw 190.6 MB / Zig 100.3 MB / WinLibs 276 MB, with licenses), Python embeddable (10.9 MB), a prebuilt runtime library with zero game code (feasible: `recomp_funcs.h` is prototypes only; the runtime references 111 public addresses), the build-time estimate (~5-15 min on a typical PC), the builder's call sequence, the hard parts, and options A/B/C.
- **Verified after the changes:**
  - All tests pass: 2,814,740 + 13 checks.
  - A full MSVC rebuild takes 96 s.
  - A scripted hidden-window run reaches the park (Pikachu meets Chatot) at a steady 30 fps.
  - `--dist` produces `SparkPark.exe` + `tools\dtk.exe` + `prompts\`.

**The Options menu in last session's test.** A repeat of the same 60 s test with no input stays on the title screen the whole time. The first run's frames show deliberate navigation: title → save slots → picture choice → back → Options → Display/Gameplay tabs. So it was real input. Likely causes:
- the game window takes focus when it opens, and anything typed elsewhere then goes to the game (WASD/arrows browse, Space/X = 2, Enter = A, V/Ctrl = B);
- or the connected controller on pad 0. Upstream reads XInput buttons even without focus; the keyboard and mouse count only with focus.

The controller is idle at rest (left stick 345/27, inside the deadzone). Test runs now use `RECOMP_GX_HIDDEN=1`, so they never take focus.

**Correction to CLAUDE.md / ARCHITECTURE.md.** The right stick already turns the camera in upstream (`read_mouse()` in `input_win32.c`, 600 px/s); the camera is not mouse-only. ARCHITECTURE.md is fixed. CLAUDE.md's "Corrections" bullet still says mouse-only, for the owner to update.

**Notes**
- `build.py --public` still assembles a `Releases/` folder whose exe contains recompiled game code. Under the distribution model it must never be published. A guard (refuse, or warn) would be a small change.
- `vk_backend.c` is shared with Android, so its application-name change also reaches the Android build. It's a string only.

**Next**
- The owner picks the builder option (A/B/C in BUILDER-PLAN.md) and approves a toolchain download for the llvm-mingw (or Zig) spike.
- Step 4 (builder) after the spike; step 5 (input: route the right stick to the pointer while DPD is on).

## 2026-11-03: Fork session 3, first release (v0.1.0-alpha, source only)

**Done**
- Committed the owner's updated CLAUDE.md (`5d5c64c`). It adds the manual right-stick camera/pointer toggle (step 5), the `--public` publishing rule, and the corrected camera note.
- **`build.py` guard** (`4e71cf0`):
  - `--public` exits before building, explains why, and runs only with `--public-local-only` (local testing, never publish).
  - `--dist` and `--android` print a "don't share or upload" reminder.
  - Tests still pass (2,814,740 + 13 checks); `--dist` still works; no `Releases/` folder is created.
- **Audit before pushing.** The only path in the whole history that matches game-data patterns is `config/R8AE01/symbols.txt` (upstream's public function list). The only binaries are the 5 builder mockups and upstream's 80 Icons8 prompt icons. The largest file is `symbols.txt` (1.9 MB).
- **README** made release-ready, plus a new **CHANGELOG.md** (`88962e3`):
  - the disc requirement spelled out (R8AE01 Rev 0, Redump SHA-1);
  - a `dtk disc verify` step before extracting, because building from source had skipped the hash check;
  - VS 2022 or 2026, and OpenGL working without the Vulkan SDK;
  - a SparkPark known-issues section;
  - the not-affiliated notice at the top;
  - the right stick listed as camera in the controls table.
- Installed the GitHub CLI 2.103.3 portable at `D:\Tools\gh` (sha256 matches the release digest). The owner signed in as Dvrkie13.
- **Published [SparkPark v0.1.0-alpha](https://github.com/Dvrkie13/SparkPark/releases/tag/v0.1.0-alpha)** as a pre-release:
  - tag `v0.1.0-alpha` → `88962e3`, pushed on its own, not with `--tags`;
  - notes drafted in `D:\SparkPark-build\release\v0.1.0-alpha-notes.md` and approved by the owner.
- **Verified after publishing:**
  - the public API reports `prerelease=true`, `draft=false`, `assets=0`;
  - the release page's Assets offers only "Source code (zip)" and "Source code (tar.gz)".

**Notes**
- `v0.1.0-beta` in the local clone is **upstream's** tag (fetched from `upstream`). Its release ships compiled game code. It was not pushed to the fork. Never use `git push --tags` here.
- The default toggle button in CLAUDE.md (Back/View/Select) is already upstream's binding for the Wii Remote's **−** (the park map and the save screen's Options button). Settle where − goes before step 5.

**Next**
- The owner picks the builder option (A/B/C in BUILDER-PLAN.md) and approves a toolchain download (llvm-mingw or Zig) for the build-time and fps spike.
- Hand-test the first-run disc check by running `SparkPark.exe` from a clean folder.
- Step 5: the right stick to the pointer while DPD is on, then the manual toggle, the setting and the indicator.

## 2026-11-04: v0.2.0-alpha, phase 1: prove the toolchain (no MSVC)

The plan is option **A** in BUILDER-PLAN.md: llvm-mingw, embedded Python, a prebuilt runtime library with no game code, and a separate builder app.

**Done**
- Downloaded, with hashes verified against the published digests, to `D:\Tools`:
  - llvm-mingw `20261020-ucrt-x86_64` (clang 23.1.4);
  - Python **3.15.2** embeddable. The 3.13 line no longer gets Windows builds; 3.13.18 is source only.
- Upstream's `recomp.py` runs unchanged under embedded Python 3.15 (11-18 s). Its output is byte-identical to the MSVC path's (all 76 files).
- New `tools/sparkpark/build_player.py`:
  - **`runtime`** (developer side) compiles `recomp/runtime`, `patches`, `boot` and `gpu` into `libsparkpark_runtime.a` (0.4 MB, 2 s). Its `recomp_funcs.h` is generated from `config/R8AE01/symbols.txt` + `hooks.txt` only (prototypes, no disc).
  - **`game`** (player side) compiles the player's generated C (clang `-O2 -std=gnu11 -w`, the flags upstream's Android build uses) and links it with the library.
- Only one source change was needed for clang/mingw: `boot.c` now includes `<mmsystem.h>` (for `timeBeginPeriod`). It's a one-line fix, safe for MSVC too, and could go upstream. Everything else compiled as is.
- **Player-style build from the ISO, no MSVC** (Ryzen 7 5800X, 16 threads):

  | Step | Time |
  |---|---|
  | verify | 6.8 s |
  | extract | 3.6 s |
  | recompile | 17.5 s |
  | compile | 36 s |
  | link | <1 s |
  | **total** | **~64 s** |

  `SparkPark.exe` is 22.3 MB (stripped). It imports only Windows system DLLs and the UCRT, **no llvm-mingw DLLs**.
- **Reaches the park.** Same script, fresh saves, hidden window: both the clang and the MSVC build go new game → Mew intro → park (Chatot). Last 30 s in the park:

  | Build | fps | game thread busy | worst frame |
  |---|---|---|---|
  | clang -O2 | 29.93 | 73.6% | 68.9 ms |
  | MSVC /O2 | 29.93 | 71.9% | 63.2 ms |

  About 2% more CPU for clang, which is within run-to-run noise. Both hold the game's 30 fps.
- **Trimmed toolchain** (x86_64 only), 299 MB:
  - `bin`: clang, clang-23, ld.lld, libLLVM-23, libclang-cpp, libc++, libunwind, plus the `mingw32-common.cfg` / `x86_64-*-windows-gnu.cfg` driver configs. Without the configs clang picks `ld` instead of lld.
  - clang's resource headers and the x86_64 builtins;
  - `x86_64-w64-mingw32/lib`;
  - the C headers (no libc++ headers).

  It builds the game (29 s), and the result runs.
- **Zip estimate**, without the builder app and font: trimmed llvm-mingw + Python + dtk + runtime library and headers + recompiler scripts + `symbols.txt`/`hooks.txt` + prompt icons = **89.4 MB zipped, 335 MB unpacked**.

**Findings for the owner**
- **Vulkan.** Without the Vulkan SDK, the runtime library is OpenGL only: its `RECOMP_VULKAN` device and the precompiled SPIR-V need `glslc` on the developer side. Upstream's default Renderer setting (Vulkan) falls back to OpenGL. To ship Vulkan, the Vulkan SDK (or just glslc) would be needed on the dev PC.
- **Picture confirmation.** "Would you like to choose this Picture?" starts on **No**. The owner reports the right stick moves it to Yes; d-pad left (keyboard Left / `RECOMP_PAD_SCRIPT` "left") works too. Scripted tests now press left before 2. Worth handling in step 5 (pointer vs d-pad menus).
- **Player disk space.** About 335 MB for the unpacked builder, 1.4 GB for the extracted disc, about 54 MB of generated C plus objects during the build, and 22 MB for the exe. Plan for ~2 GB free.

**Next (phase 2, after the owner's OK):** the builder app per `docs/launcher/sparkpark-launcher.html`. Choose WebView2 vs native first.
