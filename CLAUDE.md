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
4. **The builder.** Replace or wrap upstream's first-run disc handling with the SparkPark one-time builder (see Launcher below).
5. **Input.** Right-stick-as-pointer (upstream has no pointer support), then the base control table, then per-attraction motion mappings.
6. **Upstream gaps.** Graphics (fog, indirect texturing, dynamic lighting, logic ops, z-textures, lines/points), audio reverb/effects, Nunchuk support. Prioritize whatever is most visible in play.
7. **Polish.** Widescreen, resolution scaling, framerate options, packaging.

Upstream's known gaps (from its README at fork time): missing fog, indirect texturing, dynamic lighting, logic ops, z-textures, lines/points; no reverb/effects; silent Wii Remote speaker; no pointer or Nunchuk; Android lacks rumble and HOME; tested on one PC and one phone.

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
- The visual reference is the SparkPark Builder design mockup (to be exported and added to `docs/`).

## Open items

- Final controller mappings per attraction
- Whether SparkPark keeps upstream's Android build or goes Windows-only
