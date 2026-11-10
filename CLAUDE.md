# SparkPark — Project Brief

SparkPark is a static recompilation of **PokéPark Wii: Pikachu's Adventure** (Wii, NTSC-U) into a native Windows PC port. The original PowerPC game code is translated to C and linked against a custom runtime that replaces the Wii hardware and SDK.

**SparkPark is a fork of [MildRain479/PokePark-Wiicompiled](https://github.com/MildRain479/PokePark-Wiicompiled)**, an existing open-beta static recomp of the same build (R8AE01) that already boots and is playable on Windows (Vulkan / OpenGL) and Android. Upstream is CC0 (public domain). We build on it rather than starting over.

- Keep upstream's credit and license notices, plus the third-party licenses it lists (decomp sources, MIT/Apache disc-reading tools). Credit MildRain479 clearly in the README.
- Keep the fork mergeable: keep SparkPark changes separate and well-scoped where possible, so fixes can be pulled from upstream and offered back as PRs.
- SparkPark's job is everything on top: the one-time builder, controller support (including right-stick pointer), per-attraction motion mappings, filling upstream's known gaps, and polish.

This is a follow-up to the owner's earlier recomp project (Re-Punch!!, Punch-Out!! Wii). Reuse that project's approach and runtime pieces wherever they apply.

---

## Release status

- **v0.1.0-alpha:** source-only pre-release (rebrand, Redump verification, docs, build.py guard).
- **v0.2.0-alpha (current):** first playable release. The asset is `SparkPark-v0.2.0-alpha.zip`, the one-time builder (WebView2 UI, llvm-mingw, embedded Python, a prebuilt runtime with no game code). It builds into `%LOCALAPPDATA%\SparkPark`, needs about 2.5 GB, and took 54 s to build on an 8-core desktop. OpenGL only. Tested on one PC.
- Known issues listed at v0.2 that SparkPark owns: OpenGL only; the "Would you like to choose this Picture?" prompt starts on No; the game window takes focus on open; a controller's buttons register while the window is in the background.
- Release notes always include: upstream credit, how to play, requirements, known issues, coming next, and the unofficial / bring-your-own-disc / don't-share-built-files notices. Mention roadmap items under "Coming next".

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
| Verification | Hash the user's ISO against the Redump entry for R8AE01 Rev 0. **Refuse** another game, region or revision, or a patched/modified copy, with the reason. For an image that only differs because it's **WBFS or scrubbed**, match upstream v0.1.2: show a warning the player can continue past (these usually play fine). Use one verification path: reconcile SparkPark's `disc_verify.c` with upstream's v0.1.2 check rather than running both. |

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

**Upstream v0.1.1-beta (04 Nov) fixed:** fog, indirect texturing (both renderers), the game's audio effects incl. reverb, and in-game photos (photos taken on 0.1.0 stay scrambled; new ones are fine). It also added a startup **update check** that queries upstream's GitHub releases.

**Upstream v0.1.2-beta:** first-person view without a pointer (right-stick click on controllers, a View touch button on Android; right stick or drag looks around). This **replaces HOME** on the right-stick click. Rhyperior's Bumper Burn vertical controls fixed. A disc image check: wrong game/region/revision or a patched copy is refused with the reason; on Windows, an image that differs elsewhere (WBFS, scrubbed) gets a warning the player can continue past.

**Upstream v0.1.3-beta:** stutter fixes (60–90 ms freezes gone; the game stays on the fastest CPU core, keeps clocks from dropping, and no longer stalls on blur effects, shader compiles or graphics-memory growth; graphics memory is reserved up front). The title screen's "Please Press" shows the player's bound key/button.

**Upstream v0.1.4-beta:** Android dual-screen PokéPark Pad (friends list + zone map on the second screen) with a Dual screen Options tab; Iron Tail works from either shake key, and Primeape's tip names the shake keys; an "Unlock all passwords" button on the password screen; an Android fix for stretched Ponyta flames and tree leaves.

Remaining upstream gaps after v0.1.4: dynamic lighting, logic ops, z-textures, lines/points; silent Wii Remote speaker; no Nunchuk.

## Merging upstream

- Merge upstream releases into SparkPark regularly (`git fetch upstream`, merge the release commit). Resolve conflicts in favour of upstream's game/runtime fixes while keeping SparkPark's builder, branding, verification and guards.
- **Never push upstream's tags** (`git push --tags` is banned). Their releases ship compiled game code.
- **Update check:** upstream's checks *its own* releases. In SparkPark it must check `Dvrkie13/SparkPark` releases instead (or be off), so it never points players at a download containing game code. Keep the Options > Display > Updates toggle. It must never download anything automatically.
- **Rebuild on update:** the builder is one-time *per version*. When a new SparkPark version is unzipped, it detects that the built game is from an older version and offers a one-click rebuild from the already-verified disc (re-verify only if the ISO path or hash changed). Saves, settings and photos are always kept.

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
- Default toggle (decided 2026-11-09): **left-stick click** on a controller, **middle mouse button** on keyboard and mouse. Both were unbound after the upstream v0.1.4 merge. (Back/View/Select is the Map (−) button, and since upstream v0.1.2 the right-stick click opens **first-person view**, no longer HOME.) The toggle is remappable in settings like any other binding. Because the left stick is also used for moving, an accidental click while running is possible: ignore a click that comes with a strong stick push, or document it, when implementing.
- First-person view (upstream v0.1.2) is a pointer-style mode: in it, the right stick looks around, which should count as "camera" for the indicator and the auto-switch.
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

## Catch-up: "everything upstream has, and a little more" (decided)

SparkPark ships every upstream feature by **merging** upstream (never by re-implementing it), plus SparkPark-only extras. Next release after the merge: **v0.3.0-alpha**.

SparkPark-only extras planned on top of upstream v0.1.4:
- Everything in the QOL & performance roadmap below.
- The in-game "Unlock all passwords" button ships automatically with the merge. Nothing extra is needed.

**The PokéPark Pad (second screen) is Android-only (owner's decision).** Do NOT add it to Windows: no second-monitor Pad, no overlay. On Android, keep upstream's dual-screen Pad and Dual screen options as they are.

## QOL & performance roadmap (decided: all of it, in batches)

Do the batches in order. Within a batch, measure before and after each change. Every option is a setting (in `settings.ini`, and in the in-game overlay once it exists) with a sensible default. Nothing may break the 30 FPS original behaviour when its option is off.

**Batch 1: measure + quick wins**
- **Profiling:** a built-in frame-time breakdown (game code / graphics / audio / waiting) behind a debug flag, and a written baseline in `docs/PERFORMANCE.md`.
- **Vulkan backend (v0.2 known issue: OpenGL only):** install the Vulkan SDK on the dev machine, build with Vulkan, compare against OpenGL, and pick the default. Keep OpenGL as a fallback.
- **Widescreen 16:9:** real widescreen (wider view), not stretching. HUD and menus stay the right shape.
- **Pause + mute when the window loses focus**, and ignore keyboard/controller input while unfocused. This fixes the earlier stray-input bug and the v0.2 known issue. It's a setting, default on. Also stop the game window stealing focus when it opens.
- **Fix the Picture prompt default:** "Would you like to choose this Picture?" should start on Yes (a v0.2 known issue). Check whether it's upstream behaviour or ours first; if it's upstream's, keep the fix small and offer it back as a PR.
- **Skip intro logos/movies:** a setting, default on.
- **Borderless fullscreen + Alt+Enter.**
- **FPS counter and screenshot hotkeys:** screenshots go to a SparkPark screenshots folder.
- **Save backups (decided):** automatically copy the NAND save (and settings) to a timestamped `backups` folder before every rebuild-on-update, plus a rolling daily backup while playing. Keep the last N (setting, default 10). Add a "Restore backup" option to the builder and the in-game overlay once it exists. Never delete the live save to make room.

**Batch 2: graphics quality + speed**
- **Compiler tuning for the player's build:** `-O3`, LTO, and CPU-feature flags where safe. Benchmark them against the MSVC build.
- **Parallel compile** in the builder, using all cores, so the first build is shorter.
- **Shader/pipeline cache**, persisted on disk, to remove first-time stutter.
- **Internal resolution scaling:** 1×, 2×, 3×, 4×, and native.
- **Anti-aliasing:** off / FXAA / MSAA 2×–8×.

**Batch 3: comfort + controls**
- **Remap screen:** every action including the camera/pointer toggle, per controller type, saved per user.
- **Controller button prompts:** show Xbox/PlayStation/keyboard glyphs instead of Wii buttons in on-screen prompts where feasible. It's a setting.
- **Photo export:** save in-game photos (Pic0–Pic3 in the NAND save; correctly saved since upstream v0.1.1) as PNGs to a SparkPark photos folder, on save and via an "export all" option.
- **In-game settings overlay:** a hotkey/controller combo opens it. It holds every option above. Since the builder only runs once, this is where players change settings afterwards.

**Batch 4: texture packs + Steam Deck / Linux (decided)**
- **Texture pack support:** load replacement textures from a `textures` folder, matching textures by hash. Use **Dolphin's custom texture format/naming** so existing PokéPark Wii packs (game ID R8AE01) work unchanged. Support PNG and DDS, mipmaps, and loading packs on demand or preloading (setting). SparkPark ships no textures. Packs are made and shared by their authors, never hosted by SparkPark.
- **Texture dumping** (for pack makers): an off-by-default setting that writes the game's textures to a local `dump` folder. Dumps are game data: they stay on the player's PC, and the dump folder and any `textures` folder must be in `.gitignore` and excluded from the release zip and any crash/log reports.
- **Steam Deck / Linux, step 1 (Proton):** test the Windows release under Proton on Linux/Steam Deck. Expected trouble spots: the WebView2 builder UI under Wine, compiling with llvm-mingw under Wine (speed), controller input, and the Deck's 1280×800 (16:10) screen. Write `docs/LINUX.md` with the results and fixes, and document "add as a non-Steam game" setup.
- **Steam Deck / Linux, step 2 (native):** a native Linux x86-64 build: the runtime on Linux (SDL/Vulkan), a Linux builder using a redistributable clang + embedded Python, and a builder UI that doesn't need WebView2 (e.g. WebKitGTK showing the same launcher HTML, or a native UI). Same distribution model: no game code in the download. Release as a `.tar.gz` (consider AppImage/Flatpak later). Steam Deck controls and 16:10 must work out of the box.

**Batch 5: mod loader (decided)**
- A `mods` folder; each mod is a subfolder with a small manifest (name, author, version, description, load order). An in-game/builder list to enable, disable and reorder mods.
- **Asset mods:** replace files from the extracted disc (models, textures, audio, text) by path, layered in load order, without touching the extracted originals. Texture packs from Batch 4 become one kind of mod.
- **Code mods:** patches/hooks on game functions by address, using the symbol names from the decomp and the runtime's existing hook system. Code mods run native code: they're **off by default**, need an explicit per-mod opt-in with a clear "only install mods you trust" warning, and are disabled when the game crashes on start (safe mode).
- SparkPark never hosts or bundles mods. Mod authors are responsible for not including game data. Write `docs/MODDING.md` with the manifest format and examples.

**Batch 6: stretch goals (research first, report before building)**
- **60 FPS mode:** investigate how the game's logic depends on frame rate. Options: true 60 FPS logic where safe, or frame interpolation. Report the risks before implementing. Default off.
- **Gyro aiming** (for the pointer and tilt attractions) and **rumble**, for controllers that support them.
- **Nunchuk support.**

## Open items

- Final controller mappings per attraction
- Upstream's stutter fixes (v0.1.3) were tested on an Android handheld. Profile them on Windows in Batch 1 before assuming the same gains.
- Android: **decided, SparkPark will support it** (see "Mobile platforms"). Until that work starts, leave upstream's Android code untouched.
- iOS: on hold. It needs a Mac for building and signing.
- Vulkan SDK isn't installed on the dev machine yet, so builds are OpenGL-only for now (handled in roadmap Batch 1).
- Player builds go to `%LOCALAPPDATA%\SparkPark`, which is usually on C:. That's fine for players, but consider letting the builder choose the location (some players, like the dev, are low on C: space).
