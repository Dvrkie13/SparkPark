SparkPark v0.2.0-alpha
======================

SparkPark is an unofficial PC recompilation of PokePark Wii: Pikachu's
Adventure (Wii, USA). It is a fork of PokePark Wiicompiled by MildRain479
(https://github.com/MildRain479/PokePark-Wiicompiled, CC0), who made the game
port itself: the recompiler, the runtime, the renderers, sound and controls.

This download contains NO game data and no game code. SparkPark builds the
game on your PC, from your own disc.


What you need
-------------
- Windows 10 or 11, 64-bit.
- The Microsoft Edge WebView2 Runtime, for the builder window. Windows 11 and
  almost every Windows 10 PC have it; if SparkPark says it's missing, get it
  from https://go.microsoft.com/fwlink/p/?LinkId=2124703
- A graphics card with OpenGL 3.3.
- Your own copy of PokePark Wii: Pikachu's Adventure, USA, first release
  (R8AE01, Rev 0), as a full 1:1 disc image (ISO, RVZ, WBFS, WIA, CISO...).
- About 2 GB of free disk space for the built game, plus about 0.4 GB for
  this folder.


How to play
-----------
1. Unzip this folder anywhere.
2. Run SparkPark.exe.
3. Upload ISO: choose your disc image. SparkPark checks it against the
   original disc (Redump). If it doesn't match, it tells you why: wrong
   region, wrong revision, NKit or scrubbed, or modified.
   From a USB hard drive the check can take a couple of minutes (it reads
   the whole image); from an SSD it takes seconds.
4. Build Recomp: SparkPark extracts the disc, recompiles the game code and
   builds the game. About 1-2 minutes on a fast PC, longer on a slower one.
5. Play.

From then on, SparkPark.exe goes straight into the game.
To run the builder again (for example with another disc image), start
SparkPark.exe while holding Shift, or run "SparkPark.exe --rebuild".

Controls: keyboard and mouse or an Xbox-style controller. The builder works
with the mouse, Enter/Space or the controller's A button. In the game,
Options are in the pause menu (the PokePark Pad) and on the save file screen.


Where your game and saves are kept
----------------------------------
%LOCALAPPDATA%\SparkPark (for example C:\Users\<you>\AppData\Local\SparkPark):
  game\SparkParkGame.exe   the game, built on your PC
  game\R8AE01\             the files from your disc; your saves are in nand\
  game\settings.ini        your settings
  build.json               says the build is complete
  logs\build.log           what the builder did (look here if a build fails)

To keep everything beside SparkPark.exe instead (for example on another
drive or a USB stick), put an empty file named portable.txt next to
SparkPark.exe before the first run; SparkPark then uses a "data" folder
here.


Never share what SparkPark builds
---------------------------------
The game SparkPark builds, and the folders above, contain code and files
from your disc. They are for you to play. Don't upload or share them.


Known issues (alpha)
--------------------
- Tested on one PC so far. Please report problems on GitHub:
  https://github.com/Dvrkie13/SparkPark/issues
- The renderer is OpenGL; the Vulkan renderer isn't in this build.
- From the game port: no fog, indirect texturing, dynamic lighting, logic
  ops, z-textures, or lines and points yet; no reverb; the Wii Remote
  speaker is silent; the pointer works only in Rayquaza's and Rotom's
  minigames; no Nunchuk.
- The game window takes focus when it opens, and a connected controller's
  buttons count even while the window is in the background.


Legal
-----
SparkPark is unofficial. It is not affiliated with or endorsed by Nintendo,
Creatures Inc., GAME FREAK inc. or The Pokemon Company. PokePark Wii is
(c) Nintendo, Creatures Inc., GAME FREAK inc. and The Pokemon Company.
Bring your own disc.

SparkPark is released under CC0 1.0, as is PokePark Wiicompiled. The other
components in this folder keep their own licenses: see NOTICES.txt and the
licenses folder.
