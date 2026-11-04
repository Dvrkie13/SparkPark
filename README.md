# SparkPark

SparkPark is an **unofficial PC recompilation of PokéPark Wii: Pikachu's
Adventure** (Wii, USA, R8AE01). The game's PowerPC code is statically
recompiled into C and runs natively, with the Wii's hardware replaced
underneath it. There is no emulator: the game's own code runs, drawing with
Vulkan or OpenGL at high resolutions and frame rates, played with a keyboard
and mouse or a controller.

SparkPark is a fork of
**[PokePark Wiicompiled](https://github.com/MildRain479/PokePark-Wiicompiled)
by MildRain479** (CC0), which did the recompilation, the runtime and nearly
everything described below. SparkPark's own additions so far are its name,
a full disc check (below) and docs. Planned: a one-time builder that sets
everything up with one button, a right-stick camera/pointer toggle, and
filling some of upstream's gaps. See [Credits](#credits).

SparkPark is unofficial. It is not affiliated with or endorsed by Nintendo,
Creatures Inc., GAME FREAK inc. or The Pokémon Company.

**Bring your own disc. This repository contains no game data**, and
SparkPark never distributes any: no disc files, and no recompiled or
compiled game code. You need your own copy of the game: the USA release,
**R8AE01, Rev 0**, as a full 1:1 dump. SparkPark checks it against the
original disc's Redump hash (SHA-1
`c9a6f7601fd54b5e21a0909084839d30874a9a37`) and tells you why if it
doesn't match: wrong region, wrong revision, NKit or scrubbed, or
modified. The game is recompiled and built on your PC, from your disc.

> **Alpha, source only.** There are no ready-to-play downloads, and there
> won't be: SparkPark never ships game code. Until the one-time builder is
> done, build from source as described in [Building](#building).

## Playing on Windows

1. Build the game (see [Building](#building)).
2. Run `SparkPark.exe` (or `build/recomp/boot.exe` for a development build).
3. The first time, choose your disc image (ISO, RVZ, WBFS, WIA, CISO and
   the like). It's checked against the original disc (Redump, R8AE01
   Rev 0); if it doesn't match, you're told why (wrong region or revision,
   NKit or scrubbed, or modified). The game is then set up in an `R8AE01`
   folder beside the program, and your saves are kept in `R8AE01\nand`.

Keep the `prompts` and `tools` folders beside `SparkPark.exe`.

**Requirements:** Windows 10 or 11 (64-bit) and a graphics card with
Vulkan 1.3 or OpenGL 3.3.

## Android

Upstream's Android app is kept in the code unchanged, but SparkPark is
developed and tested on Windows only for now. See upstream's
[recomp/README.md](recomp/README.md#android) to build it.

## Improvements over the Wii version

### Graphics

- **Widescreen.** The game runs in its native 16:9 mode, with its
  widescreen pictures and layouts, at full horizontal resolution rather
  than squeezed for a TV to stretch.
- **High resolution.** Renders at 1x to 6x the Wii's resolution, or to
  fit the window. 3x is pixel for pixel on a 1440p display.
- **High frame rates.** 30 fps (the game's own), 60 fps, or 120, 144,
  165 or 240 fps. Above 60, extra frames are interpolated between the
  game's, with every object, camera and light moving smoothly. The game
  still updates 60 times a second, so nothing speeds up.
- **Anti-aliasing.** 2x, 4x or 8x MSAA.
- **Anisotropic filtering.** 2x to 16x, for sharp ground and walls at
  an angle.
- **Field of view.** 80% to 150% of the game's, in the park.
- **Vulkan and OpenGL renderers.** Vulkan is the default and the
  fastest. It falls back to OpenGL if Vulkan can't start.
- **Fullscreen** (borderless) or windowed, toggled with Alt+Enter.

### Controls

- **Keyboard and mouse**, with a **mouse-controlled camera** that turns
  and tilts the player camera, even while running.
- **Xbox-style controllers**, with rumble (off, light, normal or strong).
- **Fully rebindable.** Each Wii Remote button has two keyboard or mouse
  slots and two controller slots.
- **Button prompts match your device.** The game's on-screen Wii Remote
  icons are redrawn as the key or button they're bound to, switching
  between keyboard and controller as you use them.
- **Motion controls mapped.** The minigames' shaking, tilting, swinging
  and pointing are mapped to keys, the mouse and the sticks.
- Camera speed and inverted camera settings.

### Gameplay options

Optional changes to the game's balance, all off (100%) by default:

| Setting | Choices |
|---|---|
| Text speed | 1x (the game's), 2x, 3x, or a page at once |
| Berries collected | 50% to 300% |
| Prices | free, 50%, 100% or 200% |
| Damage dealt | 50% to 300% |
| Damage taken | none, 50% to 300% |
| Chase speed | 50% to 150% of the speed of the Pokémon you chase |

### Quality of life

- **Faster start-up.** The title screen comes up about 7 s after launch
  instead of about 28 s.
- **An in-game Options menu.** It's in the PokéPark Pad (pause menu) and
  on the save files screen, in the game's own style. Changes apply
  immediately and are saved in `settings.ini`.
- Runs at 60 Hz to match PC displays (the Wii runs at 59.94 Hz).

## Default controls

| Wii Remote | In the park | Keyboard and mouse | Controller |
|---|---|---|---|
| D-pad | Move | WASD / arrow keys | D-pad / left stick |
| 2 | Jump, talk | Space / X | A |
| 1 | Dash | Left click / Z | X |
| A | Thunderbolt | Right click / Enter | B |
| B | Recenter camera | V / Left Ctrl | Y / right trigger |
| | Turn camera | Mouse | Right stick |
| + | PokéPark Pad | P | Start |
| - | Map | M | Back |
| HOME | | Home / H | Right stick click |
| Shake | Iron Tail | Q / Left Shift (hold) | LB (hold) |
| Shake (minigames) | Running, swinging | Q and E by turns | LB and RB by turns |

While the window has focus, the mouse is captured for the camera. Esc
releases it, and a click captures it again.

<details>
<summary>Minigame controls</summary>

| Minigame | Keyboard and mouse | Controller |
|---|---|---|
| Absol's Hurdle Bounce, Bulbasaur's Daring Dash, Dusknoir's Speed Slam | Run: Q and E by turns | LB and RB by turns |
| Venusaur's Vine Swing, Tangrowth's Swing-Along | Swing: Q and E by turns, in time. Jump: Space | LB and RB. A |
| Blaziken's Boulder Bash | Swing: left click or Space. Recover: Q and E by turns | X or A. LB and RB |
| Gyarados' Aqua Dash, Empoleon's Snow Slide | Steer: A / D | Left stick |
| Pelipper's Circle Circuit, Salamence's Air Race, Rhyperior's Bumper Burn | Steer: A / D. Climb and dive: W / S | Left stick |
| Rayquaza's Balloon Panic | As flying, then aim with the mouse | Left stick, then right stick |
| Rotom's Spooky Shoot-'em-Up | Aim: mouse | Right stick |
| Bastiodon's Panel Crush | Buttons, as on the Wii | Buttons |

</details>

## Known issues

SparkPark (v0.1.0-alpha):

- **Source only.** You need Visual Studio's C++ tools and Python to build
  the game yourself; the one-time builder isn't done yet.
- **Tested on one PC, with OpenGL only.** The dev machine has no Vulkan
  SDK, so the Vulkan renderer hasn't been built or tested in SparkPark yet.
- **The disc check in the first-run window** has been tested on
  decomp-toolkit's real output for a good and a bad disc, but not yet
  clicked through by hand from a fresh folder.
- **The game window takes focus when it opens**, so anything you're typing
  elsewhere at that moment goes to the game. A connected controller's
  buttons also count while the window is in the background (upstream's
  behavior).

From upstream (the game port itself):

- **Graphics:** fog, indirect texturing,
  dynamic lighting, logic ops, z-textures, and lines and points aren't
  drawn yet.
- **Sound:** no reverb or other effects, and the Wii Remote speaker is
  silent.
- **Input:** the Wii Remote pointer works only in the minigames that need
  it (Rayquaza's and Rotom's), with the mouse or right stick; there's no
  general pointer for menus or the camera view, and no Nunchuk. The game
  is played sideways, so neither is needed for the main game.
- **Android:** no rumble yet, and no HOME button on the touch controls.
  (SparkPark keeps upstream's Android code but doesn't test it.)
- Upstream was tested on one PC and one phone, so problems on other
  hardware are likely: please report them.

## Building

Needs Windows 10 or 11, Visual Studio 2022 or 2026 with the C++ desktop
workload (the free Build Tools are enough), Python 3 and decomp-toolkit.
The [Vulkan SDK](https://vulkan.lunarg.com/) is optional: without it only
the OpenGL renderer is built, which works fine.

1. Get [decomp-toolkit](https://github.com/encounter/decomp-toolkit/releases)
   v1.6.2 and put it at `build/tools/dtk.exe`.
2. Check your disc image is the right one:
   ```
   build\tools\dtk.exe disc verify "<your disc image>"
   ```
   It should say `Redump: PokePark Wii - Pikachu's Adventure (USA)` and
   `SHA-1 : c9a6f7601fd54b5e21a0909084839d30874a9a37`, both ticked. If it
   doesn't, the image is another region or revision, NKit or scrubbed, or
   modified; make a 1:1 dump of your disc instead.
3. Extract your disc image, and put `main.dol` beside the extracted disc:
   ```
   build\tools\dtk.exe disc extract "<your disc image>" orig\R8AE01\disc
   mkdir orig\R8AE01\sys
   copy orig\R8AE01\disc\sys\main.dol orig\R8AE01\sys\
   ```
   `orig/` and `build/` are git-ignored. They hold your game data and what's
   built from it, so never commit or share them.
4. Recompile and build:
   ```
   python tools/recomp/recomp.py
   python tools/recomp/build.py --run test
   python tools/recomp/build.py --dist
   ```
   Without `--dist` you get a development build at `build/recomp/boot.exe`,
   which also has a debug settings menu (Backspace, or Back+Start on a
   controller) that can start any minigame. `--dist` puts `SparkPark.exe`
   and its files in `build/recomp/dist`.

**The program you build contains game code recompiled from your disc.
It's for you to play: never share or upload it.** For the same reason,
upstream's `build.py --public` (which assembles a `Releases/` folder) is
switched off in SparkPark; `--public-local-only` allows it for local tests.

How it works, and the environment variables for testing, are described
in [recomp/README.md](recomp/README.md) and [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Credits

- **SparkPark is based on
  [PokePark Wiicompiled](https://github.com/MildRain479/PokePark-Wiicompiled)
  by MildRain479**, released under CC0 1.0. The recompiler, the runtime,
  the renderers, the controls and the options all come from it.
- The function list (`config/R8AE01/symbols.txt`) and the decompiled
  sources in `src/` come from the
  [PokePark Wii decompilation](https://github.com/SephDB/pokepark-wii-decomp)
  by Seph De Busser and contributors.
- Disc images are read with
  [decomp-toolkit](https://github.com/encounter/decomp-toolkit) on
  Windows and [nod](https://github.com/encounter/nod) on Android, both by
  Luke Street and contributors (MIT or Apache-2.0; licenses in
  `recomp/assets`).
- Button icons by [Icons8](https://icons8.com).

PokePark Wii is © Nintendo, Creatures Inc., GAME FREAK inc. and The
Pokémon Company. This project is not affiliated with or endorsed by them.

Released under CC0 1.0 (see `LICENSE`).
