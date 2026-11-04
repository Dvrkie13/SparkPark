# SparkPark — Project Brief

SparkPark is a static recompilation of **PokéPark Wii: Pikachu's Adventure** (Wii, NTSC-U) into a native Windows PC port. The original PowerPC game code is translated to C and linked against a custom runtime that replaces the Wii hardware and SDK.

**SparkPark is a fork of [MildRain479/PokePark-Wiicompiled](https://github.com/MildRain479/PokePark-Wiicompiled)**, an existing open-beta static recomp of the same build (R8AE01) that already boots and is playable on Windows (Vulkan / OpenGL) and Android. Upstream is CC0 (public domain). We build on it rather than starting over.

- Keep upstream's credit and license notices, plus the third-party licenses it lists (decomp sources, MIT/Apache disc-reading tools). Credit MildRain479 clearly in the README.
- Keep the fork mergeable: keep SparkPark changes separate and well-scoped where possible, so fixes can be pulled from upstream and offered back as PRs.
- SparkPark's job is everything on top: the one-time builder, controller support (including right-stick pointer), per-attraction motion mappings, filling upstream's known gaps, and polish.

This is a follow-up to the owner's earlier recomp project (Re-Punch!!, Punch-Out!! Wii). Reuse that project's approach and runtime pieces wherever they apply.

---

## Hard rules (legal)

- **Never commit game data.** No ISO, DOL, RELs, extracted files, textures, audio, models, text, or disassembly output in the repo. `.gitignore` must cover all of it.
- **The user supplies their own disc.** On first run, the tool asks for the ISO, verifies it, and extracts what it needs locally.
- **Keep trademarks out of branding.** Use the name "SparkPark" for the project, repo, executable, and logo. Do not use "Pokémon", "Pikachu", or "PokéPark" in the name, logo, or icon. The README may describe it plainly as a PC recompilation of PokéPark Wii.
- No original Nintendo / Creatures / The Pokémon Company artwork in the launcher, icon, or README.

## Target build

| Field | Value |
|---|---|
| Game ID | R8AE01 (USA, Rev 0 / v1.00) |
| Only supported version | R8AE01 Rev 0. Other regions are out of scope for now. |
| Verification | Hash the user's ISO and reject anything that isn't R8AE01 Rev 0 (check against the Redump entry; `dtk disc verify` can do this). |

## Environment

- Windows PC.
- **All project files live on the D: drive** (the C: drive is low on space). Put the repo, toolchains, caches, extracted game files, and build output on D:.

## Reference projects

Base code:
- **MildRain479/PokePark-Wiicompiled**: the upstream we fork (CC0). Read its README and code before changing anything.

Symbols and structure:
- **SephDB/pokepark-wii-decomp**: matching decomp of the same build (R8AE01). Use `config/R8AE01/symbols.txt` and `splits.txt` for function names and object boundaries. Early-stage, but its naming is the best map of the binary we have.
- **encounter/decomp-toolkit (dtk)**: disc extraction, DOL/REL info, splitting, hashing.

Recompilation and runtime:
- BlackLineInteractive/NWiiRecomp
- patchzyy/Wiicompiled
- yunophilia/BlackKnightRecomp
- keepvaibin/nebula (right-stick-as-pointer reference)
- NSMBW-Community/NSMBW-Decomp (Wii SDK / NW4R / EGG reference)

## Build order (fork plan)

Steps 1–6 of a from-scratch recomp (extract, analyze, recompile, runtime, boot, overworld) are already done upstream. SparkPark's order:

1. **Fork and build as-is.** Fork upstream, build it on Windows following its README (VS 2022, Python 3, optional Vulkan SDK), everything on D:. Run it with the verified ISO. Confirm it reaches the title screen and the park is playable *before changing anything*.
2. **Map the codebase.** Write `docs/ARCHITECTURE.md`: how the disc is read, where recompiled code lives, how the runtime is laid out (graphics, audio, input, saves, SD), and how the build pipeline works.
3. **Rebrand.** Rename the executable, window title, and README branding to SparkPark per the hard rules, keeping upstream credit.
4. **The builder.** Replace or wrap upstream's first-run disc handling (`import_game()` in `import_win32.c`) with the SparkPark one-time builder (see Launcher below), including the local build toolchain.
5. **Input.** Right-stick camera/pointer split (see Input plan), then the base control table, then per-attraction motion mappings.
6. **Upstream gaps.** Graphics (fog, indirect texturing, dynamic lighting, logic ops, z-textures, lines/points), audio reverb/effects, Nunchuk support. Prioritize whatever is most visible in play.
7. **Polish.** Widescreen, resolution scaling, framerate options, packaging.

Upstream's known gaps (from its README at fork time): missing fog, indirect texturing, dynamic lighting, logic ops, z-textures, lines/points; no reverb/effects; silent Wii Remote speaker; no Nunchuk; Android lacks rumble and HOME; tested on one PC and one phone.

Corrections found while mapping the code (see docs/ARCHITECTURE.md):
- Upstream *does* have a pointer, but only in the Rayquaza and Rotom minigames (mouse or right stick). There's no general pointer for menus or photo mode. In the park, the right stick already turns the camera (its click is HOME).
- Upstream refuses the SD device; in-game photos live in the NAND save (Pic0–Pic3).
- Upstream's first-run import only checks the 6-byte game ID. It does not check revision or hash.
- A development run needs `orig\R8AE01\sys\main.dol` in addition to the extracted `disc\` folder.

## Distribution model (decided)

**The recompile and compile happen on the player's PC, from their own disc.** SparkPark releases and the repo never contain translated game code, compiled game code, or anything else derived from the disc.

- Upstream compiles on the developer's machine and ships an exe containing the translated game code. SparkPark does **not** do that, so do not publish release builds that contain game-derived code.
- The release ships: the builder, the runtime (as source or as a precompiled library with no game code in it), the recompiler, and a redistributable toolchain. Proposed: llvm-mingw (no MSVC needed, freely redistributable) plus embedded Python for the recompiler. Confirm licenses and size before committing to it.
- The builder runs: verify disc → extract → recompile → compile/link → Play. Later launches skip straight to the game.
- Developers can still build locally with MSVC as upstream does.
- Upstream's `build.py --public` assembles a `Releases/` folder whose exe contains recompiled game code. Never publish it. Keep a guard in the script that blocks this by default.

## Input plan (draft, to be confirmed)

The game is played with the Wii Remote held sideways. Base controls:

| Wii Remote (sideways) | Action | PC controller (proposed) |
|---|---|---|
| D-pad | Move | Left stick (+ D-pad) |
| 1 | Dash | X / Square |
| 2 | Jump | A / Cross |
| A | Thunderbolt | B / Circle |
| B | Recenter camera | Left trigger or shoulder |
| Point at screen + B | First-person / photo mode | Dedicated button; right stick moves the pointer |

Motion controls used in Attractions and Skill Games (needs mapping per attraction):

- **Shake to accelerate / act** (footraces, rock breaking, etc.) → mash a button or flick the right stick; optional gyro shake
- **Tilt to steer** (flying, surfing, tilt-left/right games) → left stick; optional gyro tilt
- **Swing timing** (vine swing, etc.) → button press or right-stick flick

Design the input layer so each attraction can have its own mapping table. Make gyro optional, never required.

**Right stick (decided):** in the park and normal gameplay the right stick controls the **camera** (upstream already does this). Wherever the game needs pointing (menus, first-person/photo mode, pointer minigames like Rayquaza's and Rotom's), the right stick moves the **pointer** instead. Switch modes automatically based on whether the game is reading the IR pointer. The mouse keeps working for both.

**Manual camera/pointer toggle (decided):** the player can also swap the right stick between **360° camera** and **pointer** by hand.
- Default toggle button: **Back / View / Select**. The right-stick click stays HOME as upstream has it. The toggle should be remappable in settings.
- A manual choice overrides the automatic switch until the game's pointer state changes again (e.g. leaving a menu or photo mode), then auto takes over. Add a setting for players who want the manual choice to stick: "Right stick mode: Auto / Always camera / Always pointer / Manual toggle only".
- Show a small, brief on-screen indicator when the mode changes (a camera icon or pointer icon that fades out), so the player always knows what the stick will do.
- In pointer mode, show the pointer cursor even where the game wouldn't normally draw one, so pointing is never invisible.

## Launcher (one-time builder)

The launcher is a **one-time builder**, not a menu. It appears only when no completed build exists. After a successful build, it launches the game, and every later start goes straight into the game.

Look: an original storybook-style wooden signboard in a sunny park scene. No game characters or game artwork anywhere.

- **Sign:** a framed wooden board held up by two round posts, with a log beam across the top, a beam along the bottom, and rope wraps on the posts. The "SparkPark" logo plaque (arched parchment, gold trim, glowing lightning bolt) sits on the top beam.
- **Boot animation:** the sign rises from the bottom of the screen with a bounce and sways gently once it lands. Then the logo pops in, followed by the button. A hint bar fades in along the bottom.
- **One button, centered on the board:** a banner plank with a cream inset label. It steps through:
  1. **Upload ISO** (blue): opens a file picker for the user's ISO.
  2. **Checking disc…** (spinner) while the ISO is hashed and verified.
  3. **Build Recomp** (orange)
  4. **Building… N%** with a progress bar
  5. **Play** (teal): launches the game. The builder then closes for good.
- **Caption below the button:** hidden until an ISO has been chosen. It then shows "Checking your disc…", then "✓ Disc verified · ready to build", the current build stage while building (extracting disc → recompiling game code → linking runtime), and finally "✓ Build complete · ready to play". If verification fails, say why (wrong region/revision, NKit/scrubbed, modified) and go back to Upload ISO.
- **Scenery:** a waterfall with drifting mist coming off a rock cliff in the background on the left, layered grass tufts and small flowers, and the sign's posts planted in the ground at the bottom. On the right sits a round, ragged, dented rock engraved with "Unofficial recomp of PokéPark".
- **Bottom bar:** a bold "Bring your own disc" and a single controller hint whose action follows the button (A = Upload ISO / Build / Play).
- Works with both a controller and a mouse.
- If the build fails, show the error on the sign and let the user retry.
- **The design reference is `docs/launcher/sparkpark-launcher.html`.** It's a standalone page with the exact look, animations and state flow; open it in a browser. Its build steps are simulated with timers, and the real builder wires them to the actual verify / extract / recompile / link steps. Screenshots of each state are also in `docs/`.
- Bundle the font (Baloo 2, SIL Open Font License) locally. Never load it from the internet at runtime.

## Open items

- Final controller mappings per attraction
- Android: for now, leave upstream's Android code untouched (keeps upstream merges easy) and test only on Windows. Final call still open.
- Vulkan SDK isn't installed on the dev machine yet, so builds are OpenGL-only for now.
