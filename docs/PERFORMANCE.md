# Performance

How SparkPark measures where frame time goes, and the numbers before and after each change. Measurements come from the developer's PC only. No game data, frame dumps or logs from the game go in this repo.

## How to measure

The breakdown uses upstream's two debug switches. Both are off unless set, so players never see them:

| Variable | What it does |
|---|---|
| `RECOMP_FPS=1` | Once a second it prints: fps, the slowest frame, busy % (time spent working, not waiting for the next frame), and the OpenGL call groups (draw, copy, clear, texture uploads, present) with their counts and milliseconds. |
| `RECOMP_PROFILE=<s>` | Starts a 1 kHz sampling profiler after `<s>` seconds. When the run ends (the seconds limit on the command line), it prints a "by kind" split (game code, graphics, sound, waiting, other) and the top functions. It needs symbols: a PDB beside the exe, found through `_NT_SYMBOL_PATH`. |

SparkPark's changes for profiling:

- **`boot.c`**: the "by kind" split now also counts these functions, which upstream left as "other":
  - in game code: the indirect-call lookup (`ppc_lookup`, `ppc_icall`) and the paired-single helpers (`psq_*`);
  - in graphics: the FIFO decoder's `run` and `read_floats`.
- **`tools/sparkpark/build_player.py`**: `SPARKPARK_DEBUG_INFO=1` builds the player-path (llvm-mingw) game with CodeView debug info and a PDB, so it can be profiled too. The code generated is the same. Player builds leave it off and stay stripped.

**Run setup:**
- Each run starts from a fresh save and fresh settings.
- The window is hidden (`RECOMP_GX_HIDDEN=1`) at 2× internal scale (`RECOMP_GX_SCALE=2`).
- Movies are skipped (`RECOMP_SKIP_MOVIES=1`).
- Presses are scripted with `RECOMP_PAD_SCRIPT`.
- A frame is dumped every 300 frames, to check the run reached its scene.
- The driver script is `D:\SparkPark-build\perf\run_profiles.ps1`, outside the repo.

**Machine:** Ryzen 7 5800X, Windows 11, OpenGL renderer (no Vulkan SDK yet).

**Builds compared:**
- **clang**: the player path, llvm-mingw clang 23, `-O2`.
- **MSVC**: upstream's dev build, `tools/recomp/build.py`.

## Baseline (2026-11-04, before any Batch 1 change)

| Scene | Build | fps | Frame | Slowest | Busy | Game code | Graphics | Sound | Waiting | Other |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Title screen | clang | 57.6 | 17.4 ms | 391 ms¹ | 53% | 10.9% | 39.8% | 2.7% | 37.6% | 9.0% |
| Title screen | MSVC | 59.8 | 16.7 ms | 36 ms | 58% | 9.6% | 40.2% | 1.7% | 32.6% | 15.9% |
| Park (opening forest) | clang | 29.9 | 33.4 ms | 91 ms | 69% | 12.8% | 55.6% | 2.1% | 22.6% | 6.9% |
| Park (opening forest) | MSVC | 30.0 | 33.3 ms | 46 ms | 71% | 11.9% | 54.6% | 3.2% | 21.3% | 9.0% |
| Attraction | both | — | | | | | | | | |

**Windows measured:**
- Title: the last 25 s of a 40 s run.
- Park: from 85 s to 115 s.

**Notes:**
- The park runs at the game's own 30 fps. The title screen runs at 60.
- ¹ **Clang's title hitches** (130, 216 and 391 ms) all came in the last 5 s of the run, while the title movie was playing in the background (`decode_frame`, `gx_decode_texture`). MSVC's run didn't show them. To recheck.
- **Attraction: not measured.** Entering one with `RECOMP_ATTRACTION` crashes both builds on an early save: the screen fades to white, then the game hits a guest memory fault at physical `24638004` (`load_attraction_ranking` → `ResourceManager::GetFileData` → `ParseRLB`). It happened for At 7 and At 16, and with the jump delayed from 5 s to 40 s into the park. This is upstream behaviour: the debug jump doesn't work from a save this early. Measuring an attraction needs a save that has reached one, or a fix to the jump.

### OpenGL per frame

| Scene | Draw calls | Draw | Present (swap) |
|---|---:|---:|---:|
| Title | 19 | 0.07 ms | 4.9 ms |
| Park | 245 | 1.7 ms | 13.5 ms |

Copies (EFB/XFB copies) take about 0.1 ms per frame. Texture uploads are rare once the scene is loaded.

### Top functions, park (clang)

| % | Function | Kind |
|---:|---|---|
| 45.9 | inside OpenGL (`gl`) | graphics: mostly the driver at present/swap, waiting for the GPU |
| 21.0 | `NtWaitForSingleObject` | waiting: the frame limiter and threads |
| 4.9 | `read_floats` | graphics: the GX FIFO vertex decoder |
| 2.9 | `run` | graphics: the FIFO command loop |
| 2.0 | `ai_frame` | sound |
| 1.5 | `ppc_timebase` | waiting |
| 1.2 | `memcpy` | other |
| 1.1 | `hash_bytes` | graphics: texture/state cache keys |
| 1.0 | `psq_store` | game code |

The top recompiled game function is under 1%. MSVC's list is the same shape, with `draw` at 3.4%.

### What this says

- **Graphics dominates, and most of it is inside the OpenGL driver at present.** About 13.5 of the park's 33 ms frame is spent in present, while drawing itself takes under 2 ms. GL calls are asynchronous, so present is where the CPU waits for the driver and GPU to finish the frame. Vulkan (step 2) is the most likely large win.
- **Game code is small** (about 12%). The recompiled code is not the bottleneck, so compiler choice matters little: clang and MSVC are within about 1% everywhere measured.
- **There is headroom.** The park is busy 69% of its 33 ms frame, so it would hold 30 fps on a weaker CPU. Upstream's 60 fps mode would need about 23 ms of work in a 16.7 ms frame, so it likely would not hold 60 in the park on this PC under OpenGL. This is untested: some of the busy time may scale differently at 60.
- **Sound is cheap** at 2–3%.

## Changes

Each change below adds its before/after against the baseline.

### Batch 1, step 2: Vulkan (2026-11-04)

Upstream's Vulkan renderer (`recomp/gpu/vk_backend.c`) is built in whenever the Vulkan SDK is there when the game is built. Its default setting is already `renderer = vulkan`, and it falls back to OpenGL if Vulkan can't start. The baseline was OpenGL only because no SDK was installed.

**Setup:**
- Vulkan SDK 1.4.363.1 in `D:\VulkanSDK`, found through `VULKAN_SDK`.
- GPU: AMD Radeon RX 9060 XT, Vulkan 1.4.359.
- Renderer chosen with `RECOMP_GPU=vulkan|opengl`, everything else as in the baseline.
- OpenGL was measured again with the same exe.

**SparkPark's change:** `build_player.py runtime` builds the Vulkan device into the prebuilt runtime library when `VULKAN_SDK` is set.
- It compiles SparkPark's own shaders (`recomp/gpu/vk_shaders`, upstream's, CC0) to SPIR-V with the SDK's `glslc`, the way `build.py` does.
- The player's PC needs no SDK: the game loads `vulkan-1.dll` from the graphics driver, and the game code the player compiles doesn't change.
- Without the SDK, the runtime builds OpenGL only, as before.

| Scene | Build | Renderer | fps | Frame | Slowest | Busy | Game code | Graphics | Sound | Waiting | Other | Draw/frame | Present/frame |
|---|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Title | MSVC | OpenGL | 59.7 | 16.8 ms | 35 ms | 58% | 10.0% | 39.9% | 1.2% | 33.0% | 16.0% | 0.07 ms | 4.8 ms |
| Title | MSVC | Vulkan | 59.7 | 16.8 ms | 134 ms² | **29%** | 12.3% | 9.0% | 4.2% | 55.4% | 19.1% | 0.15 ms | 0.7 ms |
| Park | MSVC | OpenGL | 30.0 | 33.3 ms | 46 ms | 73% | 12.1% | 56.2% | 1.7% | 20.1% | 9.9% | 1.7 ms | 13.5 ms |
| Park | MSVC | Vulkan | 30.0 | 33.3 ms | 44 ms | **22%** | 17.6% | 4.6% | 5.2% | 58.9% | 13.7% | 0.4 ms | 0.7 ms |
| Park | clang | Vulkan | 30.0 | 33.3 ms | 45 ms | **21%** | 18.9% | 6.5% | 2.1% | 62.0% | 10.5% | 0.4 ms | 0.7 ms |
| Park, 60 fps mode | MSVC | OpenGL | **35.8** | 28.0 ms | 42 ms | 85% | 12.1% | 65.6% | 0.8% | 12.1% | 9.4% | 1.7 ms | 13.3 ms |
| Park, 60 fps mode | MSVC | Vulkan | **60.0** | 16.7 ms | 27 ms | 36% | 19.8% | 6.9% | 5.6% | 50.8% | 16.9% | 0.4 ms | 0.7 ms |
| Park, 60 fps mode | clang | Vulkan | **59.5** | 16.8 ms | 322 ms² | 36% | 18.5% | 14.1% | 5.5% | 51.5% | 10.4% | 0.4 ms | 0.9 ms |

The 60 fps mode is upstream's `[display] frame_rate = 60`.

² **One-off slow frames on Vulkan, early in a run.** Each run starts with an empty pipeline cache. Upstream builds each new shader setup on a thread while a general shader draws, and keeps them in `vk_pipelines.bin` for the next start. These hitches fit that, but it isn't confirmed.

**What this says:**
- **Vulkan removes the bottleneck.** In the park, CPU work per frame falls from about 24 ms to about 7 ms. OpenGL's 13.5 ms present wait becomes 0.7 ms. Graphics falls from 56% of samples to about 5%. The rest is now waiting for the next frame.
- **Vulkan makes the 60 fps mode playable in the park.** It holds 60.0 at 36% busy. OpenGL manages about 36 fps.
- **Clang matches MSVC on Vulkan,** so the player build loses nothing.
- **The pictures match OpenGL's** in the frame dumps checked: the title screen and the opening forest.

**Recommendation: Vulkan as the default renderer,** with the automatic OpenGL fallback upstream already has. That means:
- keep upstream's `renderer = vulkan` default;
- ship the player runtime with the Vulkan device (releases must be packaged with `VULKAN_SDK` set);
- keep the Options menu's OpenGL choice for GPUs or drivers that misbehave.

Making 60 fps the default is Batch 4's decision, but on this PC it would hold in the park on Vulkan.

### Batch 1, steps 3, 4, 6 and 7: settings and keys (2026-11-04)

New settings, all in `settings.ini` and in the pause menu's Options under Display:

| Setting | Default | What it does | Off |
|---|---|---|---|
| `widescreen` (Widescreen) | 1 (16:9) | The game's own 16:9 mode: the console reports a 16:9 TV, and the game lays out its HUD and menus for it (not stretched). Applies from the next start. `RECOMP_WIDESCREEN` still overrides it. | 4:3, as on a 4:3 TV |
| `background_pause` (Pause in background) | 1 | While another window is in front: the game waits between frames, its clock stopped (nothing is caught up on after), the sound held, the rumble off, no input taken. | It runs behind other windows, as upstream's did |
| `show_fps` (Show FPS; also F3) | 0 | A frame rate counter in the top-left corner: a small image laid over the picture, redrawn once a second. | — |

**Keys:**
- **F12** saves a screenshot: `Pictures\SparkPark\SparkPark_<date>_<time>.png`. It holds what the window shows (menus included, the counter not). It is encoded on a thread of its own, so the game doesn't wait for the file.
- **Alt+Enter** toggles borderless fullscreen, and the Fullscreen setting does the same. Both were already upstream's (step 6 needed no change).

**Measured** (MSVC, same setup as the baseline):

| Scene | Renderer | Settings | fps | Busy | Game code | Graphics | Draw/frame | Present/frame |
|---|---|---|---:|---:|---:|---:|---:|---:|
| Park | Vulkan | before the change | 30.0 | 22% | 17.6% | 4.6% | 0.39 ms | 0.74 ms |
| Park | Vulkan | new defaults | 30.0 | 23% | 17.8% | 4.3% | 0.41 ms | 0.72 ms |
| Park | Vulkan | counter on | 30.0 | 23% | 15.7% | 4.6% | 0.42 ms | 0.73 ms |
| Park | OpenGL | counter on | 30.0 | 74% | 12.9% | 55.9% | 1.79 ms | 13.56 ms |
| Title | Vulkan | 4:3 | 60.0 | 29% | 10.9% | 8.7% | 0.07 ms | 0.71 ms |

None of these has a measurable cost: the differences are within run-to-run noise. The background pause costs nothing while the window is in front (one check per frame).

**Checked** (visible window, Vulkan and OpenGL):
- **F12** saved a PNG that matches the picture.
- **Minimizing the window** showed "SparkPark - paused" in the title, and no new frames came for 7 s.
- **Bringing it back** carried on at 60 fps.
- **The counter** shows "30 FPS" in the park on both renderers.
- **Widescreen off** gives a 4:3 window and the game's 4:3 layout.

### Batch 1, step 5: hold + to skip movies (2026-11-04)

**What the boot actually has:** no logos and no movies.
- Upstream already shortened the safety notice.
- What's left is "Hold Wii Remote Horizontally" twice (about 10 s, with the first save check between), then the title at about 14 s.
- A button press doesn't shorten Hold, and upstream's notes say leaving Hold early breaks a new game. So the boot is left as it is.

**The one long movie** is the 56 s opening after New Game (`Opening.thp`).

**The setting:** `[gameplay] movie_skip = 1` (Options → Gameplay → "Hold + to skip movies"), on by default.
- While a movie that doesn't loop is playing, holding + (whatever key or button is bound to it) for 0.8 s skips it. This doesn't happen in the movie's first second, the same guard as `RECOMP_SKIP_MOVIES`.
- While + is held, "Skipping" with a filling bar shows in the bottom-right corner. Letting go early cancels it.
- Off, + does nothing special in a movie.
- The looping title background is never skipped.

**Checked:** a scripted run started a new game and held + 10 s into the opening. "movie skipped (+ held)" followed about a second later, the indicator showed in the frames while it filled, and the game went on to the park. There's no cost outside movies (one check per movie poll).
