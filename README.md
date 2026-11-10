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
everything described below. SparkPark adds a one-time builder that checks
your disc and builds the game on your PC with one button, safe updates
(a one-click rebuild that backs up your saves first), and a few
quality-of-life additions: pausing in the background, holding + to skip a
movie, an FPS counter and screenshots. Planned: a right-stick
camera/pointer toggle, texture packs, Steam Deck/Linux, a mod loader and
Android. See [Credits](#credits).

SparkPark is unofficial. It is not affiliated with or endorsed by Nintendo,
Creatures Inc., GAME FREAK inc. or The Pokémon Company.

**Bring your own disc. This repository contains no game data**, and
SparkPark never distributes any: no disc files, and no recompiled or
compiled game code. You need your own copy of the game: the USA release,
**R8AE01, Rev 0**, as a full 1:1 dump. SparkPark checks it against the
original disc's Redump hash (SHA-1
`c9a6f7601fd54b5e21a0909084839d30874a9a37`). Another game, region or
revision, or a patched copy, is refused with the reason. A WBFS or
scrubbed copy gets a warning you can continue past, as those usually play
fine. The game is recompiled and built on your PC, from your disc.

> **Alpha.** The download from [Releases](../../releases) is the one-time
> builder, not the game: it contains no game code, and SparkPark never
> ships any. The builder makes the game on your PC from your disc.

## Playing on Windows

1. Download the latest `SparkPark-v…-alpha.zip` from
   [Releases](../../releases) and unzip it anywhere.
2. Run `SparkPark.exe`.
3. **Upload ISO:** choose your disc image (ISO, RVZ, WBFS, WIA, CISO and
   the like). It's checked against the original disc (Redump, R8AE01
   Rev 0). Another game, region or revision, or a patched copy, is
   refused with the reason. A WBFS or scrubbed copy gets a warning you can
   build past (Esc or B picks another image instead).
4. **Build Recomp:** the builder extracts your disc, recompiles the game
   code and builds the game with the compiler bundled in the zip (no Visual
   Studio needed). About a minute on a fast PC, a few on a slower one.
5. **Play.** From then on `SparkPark.exe` goes straight into the game
   (hold Shift while starting it, or run `SparkPark.exe --rebuild`, to see
   the builder again).

### Updating

Unzip the new version (anywhere, or over the old folder) and run its
`SparkPark.exe`. It finds the game the older version built and offers
**Rebuild**: one click, from the disc files already extracted, so your
disc image isn't needed. Before rebuilding, your saves and settings are
copied to `backups\<date and time>` (the newest 10 are kept). **Restore a
save backup**, at the bottom of the builder, puts one back. Your saves are
never deleted.

The game also checks SparkPark's GitHub releases at start, and the save
files screen says when a newer one is out. Options > Display > Updates
opens the release page or turns the check off. Nothing is downloaded by
itself.

### Where things are kept

The game, your saves and settings are kept in `%LOCALAPPDATA%\SparkPark`
(or in a `data` folder beside `SparkPark.exe` if you put an empty
`portable.txt` there). Everything in it comes from your disc: never share
it. The zip's `README.txt` has the details.

**Requirements:** Windows 10 or 11 (64-bit), the Microsoft Edge WebView2
Runtime (part of Windows 11 and almost every Windows 10 PC), a graphics
card with Vulkan 1.3 (or OpenGL 3.3, used if Vulkan can't start), and about
2.5 GB of free disk space (0.4 GB for the
unzipped builder, about 2 GB for the game it builds).

## Android

Upstream's Android app is kept in the code unchanged, but SparkPark is
developed and tested on Windows only for now. See upstream's
[recomp/README.md](recomp/README.md#android) to build it.

## Improvements over the Wii version

### Graphics

- **Widescreen.** The game runs in its native 16:9 mode, with its
  widescreen pictures and layouts, at full horizontal resolution rather
  than squeezed for a TV to stretch. The Widescreen setting switches it
  to 4:3 (from the next start).
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
- **Unlock all passwords.** One button on the Password screen gives that
  save file what all seven passwords give, and the game saves it.
- **Update check.** At start it asks GitHub whether a newer SparkPark
  release is out (SparkPark's own releases, never upstream's). If one is,
  the save files screen says so, and Options > Display > Updates opens its
  release page. Nothing is downloaded by itself, and the Updates setting
  turns the check off.
- Runs at 60 Hz to match PC displays (the Wii runs at 59.94 Hz).
- **Pauses in the background** (SparkPark). While another window is in
  front, the game stops, its sound goes quiet and it takes no input, then
  carries on where it was. The "Pause in background" setting turns it
  off.
- **Hold + to skip a movie** (SparkPark), such as the opening after New
  Game. A "Skipping" bar shows while you hold. Setting: "Hold + to skip
  movies".
- **Frame rate counter** (SparkPark): **F3**, or the "Show FPS" setting.
- **Screenshots** (SparkPark): **F12** saves a PNG in
  `Pictures\SparkPark`.

All of SparkPark's settings and keys are in Options (Display and
Gameplay) and in `settings.ini`. Turned off, each behaves as upstream
does.

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
| | First person | Home / H | Right stick click |
| Shake | Iron Tail | Q / Left Shift (hold) | LB (hold) |
| Shake (minigames) | Running, swinging | Q and E by turns | LB and RB by turns |

In first person, the mouse or right stick looks around and - takes a
Photo. The same button leaves it, as does any other button but + and -.

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
| Pelipper's Circle Circuit, Salamence's Air Race | Steer: A / D. Climb and dive: W / S | Left stick |
| Rhyperior's Bumper Burn | Roll: WASD | Left stick |
| Rayquaza's Balloon Panic | As flying, then aim with the mouse | Left stick, then right stick |
| Rotom's Spooky Shoot-'em-Up | Aim: mouse | Right stick |
| Bastiodon's Panel Crush | Buttons, as on the Wii | Buttons |

</details>

## Known issues

SparkPark (v0.3.0-alpha):

- **Tested on one PC so far.** Builds on slower PCs will take longer;
  please report how long yours took.
- **From a USB hard drive the disc check is slow** (it reads the whole
  image: about 2 minutes at 40 MB/s; the caption shows its progress). Copy
  the image to an SSD first for a faster start.
- **"Would you like to choose this Picture?"** starts on No: press left
  (d-pad, left stick or the arrow keys) before confirming.
- **The game window takes focus when it opens**, so anything you're typing
  elsewhere at that moment goes to the game. (While another window is in
  front, the game pauses and takes no input.)
- **Photos taken before upstream v0.1.1** (SparkPark v0.2.0-alpha and
  earlier) stay scrambled in the album; new photos are fine.
- **Entering a minigame with `RECOMP_ATTRACTION`** (a test switch) crashes
  on an early save, in upstream too. Playing the minigames normally isn't
  affected.

From upstream (the game port itself):

- **Graphics:** logic ops, z-textures, and lines and points aren't
  drawn yet (no area tested so far uses them).
- **Sound:** the Wii Remote speaker is silent. The game's own effects
  (such as reverb) are mixed in, but no area tested so far uses them, so
  that is untested.
- **Input:** no Wii Remote pointer or Nunchuk. The game is played
  sideways, so neither is needed for the main game.
- **Android:** no rumble yet, and first person needs a controller: the touch controls have no button for it.
- It has been tested on one PC and one phone, so problems on other
  hardware are likely: please report them.

## Building

Needs Windows 10 or 11, Visual Studio 2022 or 2026 with the C++ desktop
workload (the free Build Tools are enough), Python 3 and decomp-toolkit.
The [Vulkan SDK](https://vulkan.lunarg.com/) is optional: without it only
the OpenGL renderer is built, which works but is slower (see
`docs/PERFORMANCE.md`). The builder's release package needs it:
`builder/package.py stage` refuses to package an OpenGL-only runtime
unless given `--opengl-only`.

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
- The disc check follows [SparkPark](https://github.com/Dvrkie13/SparkPark)
  by Dvrkie13 (CC0).
- Button icons by [Icons8](https://icons8.com).

PokePark Wii is © Nintendo, Creatures Inc., GAME FREAK inc. and The
Pokémon Company. This project is not affiliated with or endorsed by them.

Released under CC0 1.0 (see `LICENSE`).
