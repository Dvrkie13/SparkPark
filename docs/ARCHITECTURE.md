# SparkPark architecture (upstream as forked)

This maps the code as it came from upstream, [MildRain479/PokePark-Wiicompiled](https://github.com/MildRain479/PokePark-Wiicompiled) at `f98b708`. Nothing here has been changed yet.

For the detailed behaviour and every `RECOMP_*` environment variable, see upstream's own [recomp/README.md](../recomp/README.md). This document is the map, plus where SparkPark's work plugs in.

## The big picture

```
main.dol ──tools/recomp/recomp.py──▶ build/recomp/gen/*.c   (one C function per game function)
                                          │
recomp/runtime  (CPU state, memory) ──────┤
recomp/patches  (HLE: hardware + SDK) ────┼──tools/recomp/build.py (MSVC)──▶ boot.exe / PokePark.exe
recomp/gpu      (GX decoder + GL/Vulkan) ─┤
recomp/boot     (platform: window, input, ┘
                 audio, settings, first-run import)
```

The game's PowerPC code is translated ahead of time into C. It is compiled together with a hand-written runtime that stands in for the Wii's hardware.

There is no emulator loop. The game's own code runs natively, and it calls into the replacements wherever it would have touched hardware.

## 1. How the disc is read

### First run (release build)

[`recomp/boot/boot.c`](../recomp/boot/boot.c) `find_game()` looks for `R8AE01\sys\main.dol` beside the program or one folder up. In a development build it also looks in `orig\R8AE01\sys\main.dol`.

If nothing is found, [`recomp/boot/import_win32.c`](../recomp/boot/import_win32.c) `import_game()` takes over:

1. A small Win32 window asks for a disc image (ISO, RVZ, WBFS, WIA, CISO…).
2. It runs `tools\dtk.exe disc extract -q <image> <tmp>` (decomp-toolkit, shipped beside the exe) as a child process.
3. It checks that the extracted `sys/boot.bin` starts with the game ID `R8AE01`.
4. It moves the result into `R8AE01\disc` (the files) and `R8AE01\sys\main.dol`, beside the program.

The verification checks only the ID. It doesn't check the revision or any hash.

### Every run

`boot.c` loads `main.dol` into guest memory (`ppc_load_dol`, [`recomp/runtime/runtime.c`](../recomp/runtime/runtime.c)). It takes the disc root as `<dir of sys>/disc`.

[`recomp/patches/hle_dvd.c`](../recomp/patches/hle_dvd.c) reads `disc/sys/boot.bin` and `fst.bin`. It builds a table from disc offsets to extracted files, and answers every `/dev/di` read the game's DVD library makes from those files.

### Android

Android reads the image directly, with no extraction. [`recomp/boot/disc_image_android.c`](../recomp/boot/disc_image_android.c) uses nod (Rust, built by `build.py --android`) and passes the reads through `hle_dvd_init_image`.

### Development build

The development build uses `orig/R8AE01/sys/main.dol` plus `orig/R8AE01/disc/`. The top-level README only says to extract into `orig\R8AE01\disc`, but `boot.exe` also needs `orig\R8AE01\sys\main.dol` beside it. It derives the disc root from the DOL's folder.

In this checkout, `orig/` and `build/` are directory junctions to `D:\SparkPark-data\upstream\orig` and `D:\SparkPark-build\upstream`. Game data and derived output never sit in the repo folder.

## 2. Where the recompiled code lives

### The recompiler

[`tools/recomp/recomp.py`](../tools/recomp/recomp.py) drives the recompile, with three helpers:

- [`image.py`](../tools/recomp/image.py) reads the DOL plus function boundaries from the decomp's [`config/R8AE01/symbols.txt`](../config/R8AE01/symbols.txt).
- [`translate.py`](../tools/recomp/translate.py) translates each instruction to C.
- [`regcache.py`](../tools/recomp/regcache.py) keeps GPRs in locals.

It translates all 14,407 functions plus 30 extra entry points: 766,002 instructions, none untranslated. Jump tables become `switch`es.

### The output

The output is **73 generated C files in `build/recomp/gen/`**. They are derived from the user's DOL, git-ignored, and never committed.

Each guest function `0xADDR` becomes `f_ADDR(ppc_t* c)`. Direct calls are C calls, and indirect calls go through `ppc_icall` (a binary search of the address table).

### Hooks

[`recomp/patches/hooks.txt`](../recomp/patches/hooks.txt) lists the game functions that are replaced (72 entries). For each one:

- the recompiler emits the original as `f_ADDR_orig`;
- a patch file defines `f_ADDR`;
- every call in the game, direct or indirect, reaches the patch.

This is the single mechanism for all hardware replacement and game changes.

### CPU and memory

[`recomp/runtime/ppc.h`](../recomp/runtime/ppc.h) and `runtime.c` hold the guest CPU state (`ppc_t`) and memory. Memory is one reserved 1 GB block indexed by `addr & 0x3FFFFFFF`, with byte-swapped loads and stores. MEM1, MEM2 and the locked cache are separate regions inside it.

### The decomp

[`src/`](../src) holds a few decompiled functions from the decomp (`flib/`). They are used only by the tests, to check the translator ([`recomp/tests/test_recomp.cpp`](../recomp/tests/test_recomp.cpp): 2.8 M checks).

## 3. The runtime

### Hardware and SDK replacements (`recomp/patches/`, all via `hooks.txt`)

| Area | Files | What it does |
|---|---|---|
| Threads, interrupts | `hle_thread.c`, `hle_irq.c` | Guest threads are host fibers. Interrupts (device completions, decrementer/alarms, VI retrace at 60 Hz) are delivered when the game re-enables them. |
| IOS | `hle_ios.c` | `__ios_Ipc2` is answered on the host. Devices it refuses: `/dev/sdio/slot0` (no SD card), USB Bluetooth, network. |
| **Graphics** | `hle_gx.c` → `recomp/gpu/` | Write-gather pipe stores go to a FIFO, which is decoded as soon as each command completes (`gx_fifo.c`). Also textures (`gx_texture.c`), frame recording and pacing (`gx_frames.c`), and the devices `gl_backend.c` (OpenGL 3.3) and `vk_backend.c` (Vulkan 1.3, shaders in `vk_shaders/`, only built with the Vulkan SDK). The window is `window_win32.c`. |
| **Audio** | `hle_audio.c` + `boot/audio_win32.c` | The DSP is HLE'd: it mixes AX voices (ADPCM/PCM, resampling, envelopes, LPF) on a 3 ms AI timer, and output goes out through waveOut. No aux/reverb, no remote speaker. |
| **Input** | `hle_wpad.c`, `hle_motion.c`, `hle_stick.c`, `hle_camera.c` + `boot/input_win32.c` | WPAD is replaced at the driver level (KPAD runs unchanged on top). Remote 0, held sideways, connects ~1 s in, with reports on a 200 Hz timer from `host_input.h`. `hle_motion.c` maps shake/tilt/swing/pointer per minigame. `hle_stick.c` gives analog movement. `hle_camera.c` gives the mouse camera. |
| **Saves (NAND)** | `hle_nand.c` | `/dev/fs` is backed by a host folder (`R8AE01\nand`). It seeds `SYSCONF` and `setting.txt` (US, English, 16:9). |
| **SD card** | `hle_ios.c` | **None.** `/dev/sdio/slot0` is refused, so the game sees no card. In-game photos are kept in the NAND save (`…/data/Pic0-3`). |
| Disc | `hle_dvd.c` | See section 1. |
| Other hardware | `hle_bt.c`, `hle_exi.c`, `hle_font.c`, `hle_os.c` | Bluetooth reports ready with no adapter. EXI has no devices. The system font is built from host glyphs. `OSReport` goes to stderr. |
| Movies | `hle_thp.c` | THP video is decoded natively. |
| Game changes | `hle_game.c`, `hle_modifiers.c`, `hle_pause.c`, `hle_prompts.c`, `hle_text.c` | 60 Hz timing, a shorter strap screen, gameplay sliders, the Options tab in the PokéPark Pad, button-prompt icons, and minigame instruction text. |

### Platform layer (`recomp/boot/`)

- `boot.c`: start-up, low memory as the system menu leaves it, the watchdog and crash reports.
- `settings.c`: `settings.ini` beside the exe.
- `options.c`: the in-game Options UI.
- `menu_win32.c`: the debug menu (not in public builds).
- `prompts.c`: icons from `recomp/assets/prompts`.
- Input, audio, font and import are each split into `*_win32.c` and `*_android.c`. The header contracts are in `recomp/runtime/host_*.h`.

## 4. The build pipeline

1. **Extract.** `build\tools\dtk.exe disc extract <image> orig\R8AE01\disc`. This needs dtk **v1.6.2**.
2. **Recompile.** `python tools/recomp/recomp.py`, about 17 s: `main.dol` → `build/recomp/gen/*.c`, plus the hook header.
3. **Compile and link.** `python tools/recomp/build.py`, about 2 min. This uses MSVC found through `vswhere -latest`, so VS 2026 Build Tools also work.
   - Targets: `test` (the tests) and `boot` (`recomp/boot/*.c` + `recomp/gpu/*.c` + patches + gen).
   - The Vulkan device and SPIR-V shaders are built only if `VULKAN_SDK` (or `C:\VulkanSDK\*`) is found.
4. **Options.**
   - `--run test|boot [args]`: build and run.
   - `--release`: /GL + /LTCG.
   - `--pgo`: profile-guided optimization from a run through the park.
   - `--dist`: `build/recomp/dist/PokePark.exe`.
   - `--public`: `Releases/`, which holds `PokePark.exe`, `prompts/`, `tools/dtk.exe` + license and `README.txt`.
   - `--android`: NDK + Rust/nod, producing an APK.

The recompile and compile happen on the **developer's** machine. The released `PokePark.exe` already contains the translated game code, and a player's first run only extracts the disc (section 1).

## 5. Where SparkPark hooks in

### The one-time builder (Build Order step 4)

The natural seam is `import_game()` in `import_win32.c`. `boot.c` calls it only when no `R8AE01\sys\main.dol` is found. Its contract is to produce `R8AE01\sys\main.dol` and `R8AE01\disc\` beside the program.

The SparkPark builder could:

- (a) replace `import_win32.c` with the signboard UI, adding Redump hash verification (revision, NKit/scrubbed, modified) before calling dtk; or
- (b) be a separate launcher exe that writes that folder layout and then starts the game. The game keeps working unchanged.

**Decision needed:** CLAUDE.md's builder shows "recompiling game code → linking runtime" on the player's PC. Upstream ships a prebuilt exe, and the player only extracts. Doing the build on the player's PC would mean bundling `recomp.py`, Python and a C compiler with the builder (MSVC can't be redistributed; clang/LLVM is about 300 MB+). Keeping upstream's model makes the builder an "extract and verify" step.

### Right-stick pointer (Build Order step 5)

The IR path already exists:

- `hle_wpad.c` fills WPADStatus's DPD objects (the sensor-bar dots) when the game has the IR camera on (`dpdCommand`).
- `hle_motion.c` moves the pointer from `g_host_pointer_read` (mouse, or the right stick) — but only in the minigames that use it (Rayquaza's Balloon Panic, Rotom's Spooky Shoot-'em-Up).

A general right-stick pointer would extend that gate to whenever the game enables DPD (menus, photo mode / first-person). The stick would be read in `input_win32.c` through `host_input.h`. In the park the right stick is currently free (only its click is bound, to HOME), and upstream's camera is mouse-only.

### Per-attraction mappings

`hle_motion.c` already has a per-minigame table (`{id, hold, uses}`), which is the place to extend.

### Branding (Build Order step 3)

The name "PokePark" appears in these files (from a search at fork time):

- the exe name in `tools/recomp/build.py` (`PokePark.exe`, `PokePark.apk`);
- the window class and title in `recomp/gpu/window_win32.c` (`"PokePark (recompiled)"`);
- `boot.c`, `import_win32.c`, `input_win32.c`, `settings.c`, `gl_backend.c`, `gx_frames.c`, `vk_backend.c`, `hle_ios.c` and `hooks.txt` (messages, comments, identifiers);
- Android: `AndroidManifest.xml`, the `io.github.pokeparkrecomp` Java package, `window_android.c`, `disc_image_android.c`;
- `recomp/assets/release_readme.txt` and `README.md`.

Some of these are only descriptive (comments, the README's plain description), which the hard rules allow. Step 3 has to tell those apart from real branding (the exe, window title, app name).
