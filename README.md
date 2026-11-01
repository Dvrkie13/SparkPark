# PokePark Wiicompiled

A native Windows port of *PokePark Wii: Pikachu's Adventure* (USA, R8AE01),
made by statically recompiling the game's PowerPC code into C. It renders
with Vulkan or OpenGL, plays at higher frame rates and resolutions, and
supports keyboard, mouse and Xbox-style controllers.

**This repository contains no game data.** You need your own copy of the
game: the program reads your disc image the first time it runs.

The project is in **open beta**. Please report problems in
[Issues](../../issues): say what you did and what happened, attach a
screenshot if you can, and give your Windows version, graphics card and
Renderer setting (Options > Display).

## Playing

Download the latest build from [Releases](../../releases), unzip it and
run `PokePark.exe`. The included `README.txt` has the details.

## Building

Needs Windows 10 or 11, Visual Studio 2022 (C++ desktop workload),
Python 3 and, for the Vulkan renderer, the
[Vulkan SDK](https://vulkan.lunarg.com/) (without it, only OpenGL is
built).

1. Get [decomp-toolkit](https://github.com/encounter/decomp-toolkit/releases)
   v1.6.2 and put it at `build/tools/dtk.exe`.
2. Extract your disc image:
   ```
   build\tools\dtk.exe disc extract "<your disc image>" orig\R8AE01\disc
   ```
3. Recompile and build:
   ```
   python tools/recomp/recomp.py
   python tools/recomp/build.py --run test
   python tools/recomp/build.py --public
   ```
   `--public` puts a release build in `Releases/`. Without it you get a
   development build at `build/recomp/boot.exe`, which also has a debug
   settings menu (Backspace, or Back+Start on a controller).

How it works, the settings, and the environment variables for testing
are described in [recomp/README.md](recomp/README.md).

## Credits

- The function list (`config/R8AE01/symbols.txt`) and the decompiled
  sources in `src/` come from the
  [PokePark Wii decompilation](https://github.com/SephDB/pokepark-wii-decomp)
  by Seph De Busser and contributors.
- Disc images are read with
  [decomp-toolkit](https://github.com/encounter/decomp-toolkit).
- Button icons by [Icons8](https://icons8.com).

PokePark Wii is © Nintendo, Creatures Inc., GAME FREAK inc. and The
Pokémon Company. This project is not affiliated with or endorsed by them.

Released under CC0 1.0 (see `LICENSE`).
