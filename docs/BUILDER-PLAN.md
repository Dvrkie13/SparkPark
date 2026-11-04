# Builder plan: recompile and build on the player's PC

Research only, nothing implemented. This covers how SparkPark's one-time builder (CLAUDE.md "Launcher", `docs/builder-*.png`) could turn a player's disc into a playable `SparkPark.exe` on their own PC.

Under the distribution model in CLAUDE.md, nothing derived from the disc is ever shipped. Measurements are from the dev PC (Ryzen 7 5800X, 8 cores / 16 threads) on 2026-11-03.

## What has to happen on the player's PC

| Stage | Today (upstream, dev PC) | Tool |
|---|---|---|
| 1. Verify the disc (Redump, R8AE01 Rev 0) | 6 s for an ISO (`dtk disc verify`); longer for RVZ/WBFS, which have to be decompressed | dtk 1.6.2 + `disc_verify.c` (done this session) |
| 2. Extract | about 4 s (373 files, 1.4 GB) | `dtk disc extract` |
| 3. Recompile `main.dol` → C | 17 s; output 76 files, 54 MB, 954k lines | `tools/recomp/recomp.py` (Python, standard library only) |
| 4. Compile + link | 96 s for a full MSVC `/O2 /MP` rebuild of both targets (game + tests); the game alone is a little less | MSVC today, which can't be shipped |
| 5. Launch | — | `SparkPark.exe` |

The workspace grows to about 290 MB (`gen/` 54 MB, objects 84 MB, the rest PDB/ILK debug files). A player build without debug info would need well under that.

## 1. Toolchain without MSVC

MSVC and the Windows SDK can't be redistributed, so the player needs a compiler we're allowed to ship.

Upstream already compiles the generated game code with clang: the Android build uses the NDK's clang, and `ppc.h` has `__builtin_bswap*` fallbacks. So the game side should compile with clang out of the box. Porting work is only in the Windows platform files.

| Option | Download (current) | License | Notes |
|---|---|---|---|
| **llvm-mingw** (mstorsjo), `20261020-ucrt-x86_64` | **190.6 MB** zip; on disk ~1 GB with all four targets, much less trimmed to x86_64 only | LLVM: Apache-2.0 with LLVM exceptions. mingw-w64 headers/CRT: mostly ZPL-2.1 / public domain, a few MIT/BSD files | clang + lld + the mingw-w64 headers and import libs: d3d11, dxgi, WIC, dbghelp, winmm, ole32 are all there. Well-trodden. **Recommended.** |
| **Zig** 0.17.1 (`zig cc`) | **100.3 MB** zip | MIT (bundles LLVM, Apache-2.0, and mingw-w64) | One self-contained zip, smaller, and caches builds. Its clang version is tied to Zig releases, and its mingw-w64 import-lib coverage needs checking for d3d11/dxgi/WIC. Good fallback. |
| WinLibs GCC 16.2 (mingw-w64 UCRT) | 276.4 MB | GPLv3 with the GCC runtime exception (output is unencumbered) | Works, but it's larger and gcc is usually slower than clang on huge generated C. |
| clang-cl + `xwin` (fetches Microsoft's CRT/SDK on the player's PC) | ~1 GB of Microsoft downloads | Microsoft's license terms, which each player must accept | Legally murky for a one-click builder. **Not recommended.** |
| TCC | <1 MB | LGPL | Tiny and fast to compile, but no optimizer: the game would run several times slower. No. |

### Porting the Windows side to clang/mingw

1. `#pragma comment(lib, …)` in `boot.c`, `import_win32.c`, `prompts.c`, `font_win32.c`, `audio_win32.c`, `gl_backend.c`. clang honours it for COFF, but whether the mingw link of ld.lld picks those libraries up needs checking. The safe route is explicit `-l` flags in the player build script.
2. `<intrin.h>` in `runtime.c` exists in mingw-w64.
3. Two `_MSC_VER` branches (`ppc.h`, `vk_backend.c`) already have a non-MSVC side.
4. `StackWalk64`, `MiniDumpWriteDump` and `SetUnhandledExceptionFilter` (crash reports in `boot.c`) need dbghelp, which mingw-w64 ships.
5. WGL / DXGI / `WGL_NV_DX_interop2` (`gl_backend.c`): the headers are in mingw-w64; to be checked.

The changes stay small and in Windows-only files, so upstream merges keep working. The MSVC developer build stays as it is.

### Performance

The release build would lose MSVC's `/LTCG` and PGO. Upstream measured PGO at about 8% per frame. clang `-O2`, or `-O2 -flto=thin`, is likely in the same range as MSVC `/O2`, but **this needs measuring**.

A PGO profile from a dev playthrough contains only function and branch counts, not game code. Whether shipping it counts as "derived from the disc" is a call for you.

## 2. Shipping the recompiler

`recomp.py`, `translate.py`, `regcache.py` and `image.py` use only the standard library (argparse, struct, re, bisect, …).

| Option | Size | License | Trade-off |
|---|---|---|---|
| **Python embeddable package** (python.org `embed-amd64` zip, 3.13) | **10.9 MB** zip, about 25 MB unpacked | PSF, redistributable | The builder runs `python.exe tools\recomp\recomp.py`. Runs upstream's scripts unchanged, so it's merge-friendly. **Recommended.** |
| PyInstaller / Nuitka single exe | ~8-15 MB | GPL with exception / Apache | One file, but these executables are a frequent antivirus false positive. |
| Port the recompiler to C | small | ours | Native and fast, but about 1,000 lines to rewrite, and the fork drifts from upstream's recompiler. No. |

## 3. A prebuilt runtime with no game code

**Yes, this looks feasible.**

- The game side is everything in `build/recomp/gen/`: the translated functions and `recomp_table.c` (the address table `ppc_icall` searches).
- The runtime side (`recomp/runtime`, `patches`, `boot`, `gpu`) contains no game code. It uses the generated header `recomp_funcs.h`, but that header is only prototypes (`void f_80004000(ppc_t* c);`, 14,509 of them), plus `#include "ppc.h"`.
- The runtime calls **111** game functions (hooked bodies and their `_orig`). Their addresses come from the public decomp's `symbols.txt` and `hooks.txt`, not from the disc.

Plan:

- Build `sparkpark_runtime.a` with the same llvm-mingw, from a `recomp_funcs_public.h` generated from `symbols.txt` + `hooks.txt` alone. That header can be generated without a disc. Alternatively, declare just the 111 functions the runtime uses.
- Precompile the Vulkan SPIR-V from upstream's shaders on the dev machine, so the player needs no Vulkan SDK.
- On the player's PC: recompile → compile `gen/*.c` → link with the runtime library.

Constraints:

- **Same `hooks.txt` on both sides.** The recompiler emits `f_X_orig` only for hooked functions, so the runtime library and the player's recompile must use the same version. Stamp a version into both and check it at link time.
- **Same defines (`RECOMP_TRACE`, …) on both sides.** `ppc.h` macros are inlined into the generated code.
- **Same toolchain for library and game objects.** An MSVC `.lib` can't simply be linked by mingw/lld because the C runtimes differ.

## 4. Estimated build time on a player's PC

| Stage | Dev PC (16 threads) | Typical player PC (4-8 threads, estimate) |
|---|---|---|
| Verify (ISO / RVZ) | 6 s / ~30-60 s | 10 s / 1-2 min |
| Extract | 4 s | 10-30 s |
| Recompile (Python) | 17 s | 20-40 s |
| Compile + link (clang -O2, game only) | ~1.5-3 min (estimate: clang is often 1-2x MSVC on huge generated C) | **~3-12 min** |
| **Total** | **~2-4 min** | **~5-15 min** |

The compile estimate is the shakiest number. **A spike is needed**: build upstream's game with llvm-mingw on this PC and time it.

Lower optimization for the biggest generated files, `-O1`, would cut compile time at some speed cost. ThinLTO would add time.

## 5. What the builder UI would call

The mockups' button steps map to these stages:

1. **Upload ISO.** A file picker (filters as in `import_win32.c`).
2. **Checking disc…** `dtk disc info` + `dtk disc verify` → `disc_check_info` / `disc_check_verify` (`recomp/boot/disc_verify.c`). Caption: "✓ Disc verified · ready to build", or the reason (wrong region or revision, NKit/scrubbed, modified). Progress comes from dtk's output, or a spinner.
3. **Build Recomp.**
4. **Building… N%**, three stages:
   - **Extracting disc.** `dtk disc extract` into `R8AE01\disc` + `sys\main.dol`, about 10% of the bar.
   - **Recompiling game code.** `python\python.exe tools\recomp\recomp.py`, about 10%. It would need a progress line per output file; that's a small addition to recomp.py, upstreamable.
   - **Linking runtime.** clang on each `gen/*.c` (run in parallel, one job per core; progress = files done / 76), then lld with `sparkpark_runtime.a`, about 80%.

   Writes a completion marker (a build manifest with the disc hash, the SparkPark version and `hooks.txt` version) and deletes intermediate objects.
5. **Play.** Launch `SparkPark.exe`. On later starts, a valid manifest means the builder is skipped. A SparkPark update with a newer runtime or hooks version triggers a rebuild, reusing the extracted disc.

The simplest structure is a small separate **builder exe** that owns the UI and spawns dtk/python/clang. That keeps upstream's `boot.c` / `import_win32.c` nearly untouched, and the game exe stays the same program a developer builds. The alternative is putting the UI inside the game exe's `import_game()`, but the game exe doesn't exist until the builder has built it, so the builder **must** be a separate program anyway.

## What makes this model hard

1. **Download size.** Builder + runtime library + dtk (~9 MB) + Python (~11 MB) + toolchain (~100-190 MB) comes to roughly **130-220 MB zipped**, against upstream's ~30 MB release.
2. **Antivirus / SmartScreen.** An unsigned builder that unpacks a compiler and writes a fresh unsigned exe is a classic false-positive pattern. Code-signing would help, at a cost.
3. **A second build path to maintain.** There's MSVC (dev, upstream) and llvm-mingw (player). Each upstream merge has to keep both building, so CI that builds both from a *disc-free* dummy would help. The recompiler can't run without a DOL, so CI could only build the runtime library.
4. **Performance difference.** Losing PGO/LTCG may cost several percent. It needs measuring.
5. **Version coupling.** The runtime library, `hooks.txt` and `recomp.py` versions must match the player's build, so every SparkPark update means a rebuild on the player's PC (a few minutes; the disc stays extracted).
6. **Debuggability.** Every player's exe is built locally, so crash reports need the exact toolchain and version pinned (shipped) to be reproducible.

## Options

- **A. Recommended.** llvm-mingw (trimmed to x86_64) + embedded Python + prebuilt `sparkpark_runtime.a` + a separate builder exe.
  1. Spike: build the game with llvm-mingw and measure time and fps.
  2. If it works, add a `tools/sparkpark/build_player.py` that does the player build. `build.py` stays untouched.
- **B. Smaller download.** As A, but with Zig instead of llvm-mingw: about 100 MB, if its mingw import libs cover d3d11/dxgi/WIC.
- **C. Simplest first version.** Ship no runtime library; compile the runtime sources on the player's PC too. About 30 s more build time, but no ABI/version coupling. It could be the first milestone, with A's prebuilt library later.

**Decision needed from you:** pick A, B or C, and approve the llvm-mingw (or Zig) download for the spike.
