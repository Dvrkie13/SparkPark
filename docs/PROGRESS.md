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

## 2026-11-04: v0.2.0-alpha, phase 2: the builder app (WebView2)

**Done**
- **UI: WebView2**, the owner's choice. `builder/make_ui.py` generates `builder/ui/index.html` from the design reference `docs/launcher/sparkpark-launcher.html`. The look and animations are untouched. It only changes four things:
  - the Google Fonts link becomes a local `@font-face` for `fonts/Baloo2-wght.ttf` (Baloo 2, SIL OFL 1.1, from google/fonts; file hashes match the repo's git blobs);
  - the stage scales to fit the window;
  - the Build label gets an id (for "Retry Build");
  - the timer script becomes `builder/ui_script.js`.
- **`builder/ui_script.js`**: the same state machine, driven by the launcher.
  - Captions follow the real stages (extracting disc → recompiling game code → linking the runtime).
  - A failed disc returns to Upload ISO with the reason in a red caption pill.
  - A failed build shows **Retry Build** with the reason.
  - Controller support: A (Gamepad API, standard mapping) presses the button, as do Enter/Space and the mouse.
- **`builder/launcher.cpp`** (`SparkPark.exe`, 0.4 MB): built with MSVC on the dev side by `builder/build_launcher.py`, with the static WebView2 loader (SDK 1.0.4314.35, BSD-style licence) and the static CRT. It needs only the WebView2 Runtime on the player's PC.
  - **Pick:** a file picker → `dtk disc info` + `verify` → `disc_verify.c`. The sign gets a short reason per verdict; the full text goes to the log.
  - **Build:**
    - dtk extract, with progress from bytes written against the known 1,400,709,637;
    - the embedded Python running `recomp.py`, with progress from files written against 73;
    - the embedded Python running `build_player.py game`, with progress from its `[n/76]` lines;
    - then the prompt icons are copied and `build.json` is written.
  - **Play:** starts the game and closes. **Every later start goes straight into the game.** `--rebuild`, or holding Shift, shows the builder again.
  - If the WebView2 Runtime is missing, a message points to Microsoft's installer.
- **Where things go (SPARKPARK_HOME):** `%LOCALAPPDATA%\SparkPark` by default, `<SparkPark folder>\data` when `portable.txt` sits beside the launcher, or `%SPARKPARK_HOME%`. Inside it:
  - `game\SparkParkGame.exe`;
  - `game\R8AE01\{disc,sys,nand}` (saves in `nand`);
  - `game\settings.ini`;
  - `build.json` (the "build complete" marker);
  - `logs\build.log`;
  - `webview2\` (the page's browser cache);
  - `work\`, which is deleted after a build.
- **`builder/package.py stage`** assembles the app folder (351 MB): launcher, page and font, dtk, prompt icons, the recompiler scripts + `symbols.txt` + `hooks.txt` + runtime headers, the trimmed llvm-mingw, embedded Python, and the runtime library (built from the disc-free header with the full llvm-mingw).
- **Test hooks** (environment only): `SPARKPARK_TEST_ISO`, `SPARKPARK_AUTOPRESS`, `SPARKPARK_SHOTS`, `SPARKPARK_NO_LAUNCH`, `SPARKPARK_TEST_FAIL=compile`.
- **Tested in portable mode on D:**
  1. **A one-byte-modified ISO** (same size, correct header) was rejected as *modified* (Redump: not found).
  2. **A missing compiler** (`SPARKPARK_TEST_FAIL=compile`) gave "The bundled compiler couldn't be started…" with Retry Build.
  3. **The real ISO.** The full flow, then a 74 s build, then Play. The launcher closed and started the game, which reached the park (Chatot) with the scripted left+2 presses.
  4. **The second start** went straight into the game: the launcher exited after 77 ms with no builder page.

  Screenshots of every state are in `D:\SparkPark-build\shots\builder-states.png`.

**Not yet tested by hand:** the real file picker, a mouse click, and a physical controller's A button. Automated runs used the test hooks.

**Next (phase 3, after the owner's OK):** licences/notices for every component, README.txt, the zip, the audit, and a clean-folder test as a new player.

## 2026-11-04: v0.2.0-alpha, phase 3: package, audit, clean test

**Done**
- **Fixes from the owner's own run.**
  - From a USB hard drive (about 40 MB/s), "Checking disc…" sat for about 2 minutes with nothing moving. The caption now shows how far dtk has read ("Checking your disc… 66%", from its process I/O counters).
  - Closing the builder mid-build left Python running; all children now run in a kill-on-close job object.
  - `render()` had been clearing the design's caption pill background. Fixed, and checked against `docs/builder-*.png`.
- **README.txt** (player guide: requirements, steps, where data goes, portable mode, `--rebuild`/Shift, never share built files, known issues, legal) and **NOTICES.txt** (every component, its folder, licence and source). Also `licenses/` with SparkPark/upstream CC0, the decomp's CC0, decomp-toolkit, llvm-mingw (LLVM), mingw-w64's COPYING files, Python, Baloo 2 OFL, the WebView2 SDK and an Icons8 note. mingw-w64's notices are also kept in `toolchain\llvm-mingw\x86_64-w64-mingw32\share\mingw32\`.
- **`SparkPark-v0.2.0-alpha.zip`**: 93.3 MB, 3,514 files, 351 MB unpacked, sha256 `fa461123…690d089`. Built by `builder/package.py stage` + `zip`.
- **Audit** (`builder/audit.py`; report and full file list in `D:\SparkPark-build\release-0.2.0\audit\`). **PASS:**
  - 0 files identical to any of the disc's files or `main.dol`.
  - **0 files containing main.dol code.** All 2,670,002 non-trivial 32-byte windows of its code sections were searched, which catches any copy of 48+ bytes.
  - main.dol data matches are only trivial tables. The longest run in files made for this release is 384 bytes (MSVC CRT's `80 81 … FF` identity table in SparkPark.exe). Others: the font's ascending glyph numbers, the standard JPEG zig-zag table in the runtime, and class names in the public `symbols.txt`/`hooks.txt`. Official downloads show at most 512 bytes (256-entry 16-bit lookup tables in Python's DLLs). Two code-section hits during development were the Metrowerks TRK banner string in dtk.exe.
  - 0 recompiler output markers outside the recompiler's own source; 0 generated files; 0 game-format extensions or signatures.
  - The runtime library has 39 objects, all from `recomp/runtime|patches|boot|gpu`. It defines only the 72 hand-written hooks of `hooks.txt` and refers to 39 game functions by address.
  - Every exe/dll/pyd is byte-identical to its official download (llvm-mingw, Python, dtk), except SparkPark.exe.
- **Clean test**, as a new player: Expand-Archive into a new `D:\SparkPark-cleantest`, then `SparkPark.exe`. To keep data on D: per the owner's rule, `portable.txt` was added; by default data goes to `%LOCALAPPDATA%\SparkPark`.
  - Disc check, then a 54 s build, then Play. The game reached the park (Chatot).
  - The second start went straight into the game (97 ms, no builder page).
  - The work folder was removed after the build; the data folder is 1.42 GB.
  - Automated: the file picker (`SPARKPARK_TEST_ISO`) and the button presses (the page's own `press()`).

**Next (phase 4, after the owner's OK):** draft the release notes, the asset list and the exact gh commands; publish only after a yes.

## 2026-11-04: v0.2.0-alpha, phase 4: release

**Done**
- README and CHANGELOG updated for the builder download (`c1ee58a`).
- Pushed `main` (`4a8e022..c1ee58a`), then the **`v0.2.0-alpha`** tag on its own (`git push origin v0.2.0-alpha`, never `--tags`). The fork's tags are only `v0.1.0-alpha` and `v0.2.0-alpha`.
- **Published [SparkPark v0.2.0-alpha](https://github.com/Dvrkie13/SparkPark/releases/tag/v0.2.0-alpha)** as a pre-release with the owner-approved notes (`D:\SparkPark-build\release-0.2.0\v0.2.0-alpha-notes.md`).
- **Verified as a visitor:**
  - The public API reports prerelease=true, draft=false, **1 asset**: `SparkPark-v0.2.0-alpha.zip`, 93,306,079 bytes, GitHub digest sha256 `fa461123…690d089`.
  - A fresh download matches the audited zip byte for byte.
  - The release page lists only that zip plus GitHub's automatic "Source code" archives.

**Next**
- Feedback from players: build times on other PCs, the WebView2 runtime on older Windows 10, and the picker/mouse/controller by hand.
- Build Order step 5: right-stick camera/pointer (auto-switch while the game has DPD on, manual toggle, "Right stick mode" setting, indicator). Settle the Back-button conflict with Wii − first.
- Vulkan in the builder's runtime needs `glslc` (Vulkan SDK) on the dev PC.

## 2026-11-04: Roadmap Batch 1 (profiling, Vulkan, quality of life)

Numbers and methods: `docs/PERFORMANCE.md`.

**Done**
1. **Profiling.**
   - Upstream's `RECOMP_FPS` and `RECOMP_PROFILE` are the frame-time breakdown, behind environment flags.
   - `SPARKPARK_DEBUG_INFO=1` gives the clang build a PDB. The profiler's split counts four helpers it missed.
   - **Baseline** (OpenGL, park): 30 fps at 70% busy. Graphics is 55% of samples, mostly a 13.5 ms present wait in the driver; game code is 12%. Clang and MSVC are within 1%.
   - **Attraction: not measured.** `RECOMP_ATTRACTION` crashes both builds on an early save (a guest fault in `load_attraction_ranking`, upstream behaviour).
2. **Vulkan.**
   - Vulkan SDK 1.4.363.1 in `D:\VulkanSDK` (the owner ran LunarG's signed installer).
   - Upstream already defaults to Vulkan, with a fallback to OpenGL.
   - Park: 22% busy against 73%. The 60 fps mode holds 60.0 against OpenGL's 35.8.
   - **Recommendation: keep Vulkan as the default.**
   - `build_player.py runtime` now builds the Vulkan device into the shipped runtime. Players need no SDK.
   - `package.py stage` refuses an OpenGL-only runtime without `--opengl-only`.
   - `NOTICES.txt` and `licenses/` cover the Vulkan headers (MIT).
3. **Widescreen.** Upstream's native 16:9 mode (HUD and menus laid out by the game, not stretched) is now the Widescreen setting. On by default; 4:3 when off, from the next start.
4. **Pause in background** (on).
   - At each frame, while another window is in front, the game waits with its clock stopped, its sound held, rumble off and no input taken.
   - `ppc_timebase_pause` now works on Windows too.
5. **Hold + to skip movies** (on), as the owner asked: hold + (P / Start) for 0.8 s; a "Skipping" bar shows in the bottom right.
   - The boot itself has no logos or movies. Upstream already shortened the strap screen, and Hold must run its course, so the boot is unchanged.
6. **Borderless fullscreen and Alt+Enter:** already upstream's. Checked through the launcher (2560x1440).
7. **FPS counter** (F3 / Show FPS) and **F12 screenshots** (PNG in `Pictures\SparkPark`, written on a thread).
   - The counter and the skip bar are small images laid over the picture, in both renderers.
   - Neither cost is measurable.
- **The player path:**
  - Launcher rebuilt. App staged (352.5 MB) and zipped (93.6 MB, not released).
  - **Audit PASS**: 0 game code, 0 disc data. The runtime has 40 objects, `hud_win32` among them.
  - A fresh run: disc check, build (27 s compile), play. New game, + held through the opening, on to the park, all on Vulkan.

**Next**
- Release notes and a version bump for the next release, after the owner's OK.
- Batch 2 (graphics options). Several items in it are already upstream's (scale, MSAA, the Vulkan pipeline cache), so it's mostly FXAA and settings polish.
- The `RECOMP_ATTRACTION` crash on early saves, if attraction profiling is wanted.

## 2026-11-09: v0.3.0-alpha, phase 1: upstream v0.1.4-beta merged, update check, rebuild on update, backups

On branch `merge-upstream-0.1.4` (not pushed).

**Merge**
- `git fetch upstream --no-tags`, merged `9b5804e` (v0.1.4-beta): 37 commits, 46 files. `20debaf`, a later README-only commit, is not included.
- Eight files conflicted. All resolutions are listed in the merge commit (`d686354`):
  - **`disc_verify.c/.h`:** upstream's version, which credits SparkPark's.
  - **`import_win32.c`:** upstream's, with SparkPark's texts re-applied.
  - **`host_settings.h`, `settings.c`:** both sides' settings kept.
  - **`hle_wpad.c`:** upstream's `hle_wpad_held()` replaces our `g_wpad0_held`.
  - **`vk_backend.c`:** upstream's second screen and our HUD both kept.
  - **`build.py`:** upstream's test list.
  - **`README.md`:** our builder steps and Android note, upstream's known issues.
- Built with MSVC and llvm-mingw. Unit tests: 2,814,756 checks, 0 failures.

**SparkPark changes**
- **Update check** (upstream's `update.c`):
  - It now asks `Dvrkie13/SparkPark`'s releases, pre-releases included (`releases/latest` skips them).
  - It compares with `SPARKPARK_VERSION` ("0.3.0-alpha", `recomp/runtime/sparkpark_version.h`), which the builder uses too.
  - It still only opens the page and never downloads anything. Upstream's `version.h` is untouched.
- **One disc check** (upstream's `disc_verify.c`, the same code as Android):
  - another game, region or revision is refused with the reason;
  - an image that isn't Redump's gets an amber warning; build anyway, or Esc / B to pick another;
  - a patched `main.dol` is refused after extracting.
- **Rebuild on update:**
  - When `build.json` is from another version, the builder opens on Rebuild, from the extracted disc files (byte count and `main.dol` checked), with no disc needed.
  - If those files are missing, it asks for the disc again.
  - v0.2.0's `build.json` doesn't record the image's path, so the extracted files are the only way to make it one click.
- **Save backups:** before any build over a save, `nand` and `settings.ini` go to `backups\<date_time>` (newest 10 kept). "Restore a save backup" on the page backs up the current save first.
- `build_player.py` links `winhttp`.

**Checked** (screenshots in `D:\SparkPark-build\v03\shots`, test installs in `D:\SparkPark-build\v03`)
- **Disc check on the owner's ISO:** original (Redump), 127 s. An altered copy got:
  - a changed byte near the end: the warning;
  - R8AP01: refused ("European release");
  - revision 1: refused;
  - RSPE01: refused ("another game");
  - patched `main.dol`: refused after extracting.
- **Zip:** 93.8 MB, 3,517 files. **Audit PASS:** 0 game code, 0 disc data. The runtime has 43 objects, `update` and `friends` among them.
- **Through the launcher** (the test zip, the owner's ISO): build in 249 s.
- **Title screen:** "Please Press" shows the bound key (Space).
- **Update check:** "Update: v0.2.0" over Options when run as 0.1.0. The log says "the latest release is v0.2.0-alpha".
- **Password screen:** an Unlock all button. `-` → "All passwords entered!" (the log: "passwords: file 0 unlocked").
- **First person on HOME:** sights in the middle; HOME again leaves it. The mouse turns the camera (shown on the normal camera; first person and the mouse together not captured).
- **Fog:** subtle in the opening forest (a little more haze).
- **Upgrade from v0.2.0** (its release zip, a new save, v0.3.0 beside it on the same data folder):
  - the rebuild was offered from the extracted disc;
  - backup `2026-11-09_19-33-06` was made;
  - the rebuild took 40 s;
  - every save file's hash was unchanged;
  - the rebuilt game shows "Game 1".
  - Restore put a backup back after a change, and backed up the changed save first.

**Not checked, and why**
- **Indirect texturing** (Gyarados' Aqua Dash) and **Rhyperior's up/down:** `RECOMP_ATTRACTION` still crashes on an early save (`load_attraction_ranking`, upstream's), and no save that has reached them exists.
- **A photo in the album:** on a new game `-` in first person saved nothing (the photo files hold only their headers). The camera likely comes later in the story.
- **Iron Tail from either shake key, and Primeape's line:** Iron Tail is only in battles, and the line only appears after Primeape teaches it. Upstream checked it by what the scripts read.
- **Reverb:** upstream: "No area tested so far turns an effect on."

**Notes**
- On Windows the dual-screen Pad isn't reachable: there's no Dual screen tab. Upstream's developer switch `RECOMP_GX_SECOND` can still draw it off-screen for frame dumps.
- "Would you like to choose this Picture?" still starts on No (Batch 1's fix, not done yet).

## 2026-11-09: v0.3.0-alpha, phase 2: controls

**Decided:** Batch 1 (already on `main`) ships in v0.3.0-alpha.

**Every binding after the merge** (defaults; all rebindable in Options, two slots each)

| Wii Remote | Action | Keyboard / mouse | Controller |
|---|---|---|---|
| D-pad | Move | WASD / arrows | D-pad, left stick (always) |
| 2 | Jump, talk, confirm | Space / X | A |
| 1 | Dash, back | Left click / Z | X |
| A | Thunderbolt | Right click / Enter | B |
| B | Recenter camera | V / Left Ctrl | Y / RT |
| + | PokéPark Pad; hold to skip a movie | P | Start |
| − | Map; Options on the save file screen; photo in first person; Unlock all on the password screen | M | Back |
| HOME | First-person view (upstream v0.1.2) | Home / H | Right-stick click |
| Shake | Iron Tail; minigame running/swinging | Q / Left Shift | LB |
| Shake 2 | The minigames' other shake | E | RB |
| (camera / pointer) | Turn the camera; aim in Rayquaza's and Rotom's minigames and first person | Mouse | Right stick |

**Fixed keys** (not rebindable):
- **Alt+Enter:** fullscreen.
- **F3:** FPS counter. **F12:** screenshot.
- **Esc:** releases the mouse; a click captures it again.
- **In the Options tab:** arrows, Enter, Esc, Delete / Y (clear a binding).
- **In the builder:** Enter/Space/A for the button, Esc/B to go back after a disc warning.
- **Developer builds only:** Backspace or Back+Start opens the debug menu.

**Unbound by default:** controller LT and left-stick click; keyboard Tab, Caps Lock, middle mouse button, most letters and F-keys.

**Camera/pointer toggle (decided by the owner):** left-stick click and the middle mouse button. Written into CLAUDE.md; not implemented yet.

## 2026-11-09: v0.3.0-alpha, phase 3: release

**Done**
- README, CHANGELOG (v0.3.0-alpha) and the zip's README.txt/NOTICES.txt updated (`8e26aba`).
- Release zip built from `8e26aba`: `SparkPark-v0.3.0-alpha.zip`, 93,793,943 bytes, 3,517 files, sha256 `b97e8b6b…bd95b28d0`.
- **Audit PASS:** 0 game code, 0 disc data. The report is in `D:\SparkPark-build\release-0.3.0\audit\`.
- `main` fast-forwarded to the merge branch and pushed (`4b3ecbc..8e26aba`).
- Only the `v0.3.0-alpha` tag pushed, never `--tags`. The fork's tags are `v0.1.0-alpha`, `v0.2.0-alpha` and `v0.3.0-alpha`.
- **Published [SparkPark v0.3.0-alpha](https://github.com/Dvrkie13/SparkPark/releases/tag/v0.3.0-alpha)** as a pre-release, with the owner-approved notes (`D:\SparkPark-build\release-0.3.0\v0.3.0-alpha-notes.md`).
- **Checked as a visitor:**
  - The public API shows prerelease=true, draft=false, and **1 asset**, the zip, with GitHub's digest matching the audited sha256.
  - A fresh download is byte-identical.
  - The page has nothing else but GitHub's automatic "Source code" archives.
  - SparkPark's update check now sees `v0.3.0-alpha` as the newest release.

**Next**
- The camera/pointer toggle (left-stick click, middle mouse button) and the right stick as a pointer.
- The rest of Batch 1's list in CLAUDE.md: the Picture prompt starting on Yes, not taking focus at start, rolling daily backups.
- The `RECOMP_ATTRACTION` early-save crash, which blocks checking Aqua Dash's indirect texturing and Rhyperior's controls in tests.
