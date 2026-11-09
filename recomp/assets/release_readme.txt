SparkPark (open beta)

An unofficial PC recompilation of PokePark Wii: Pikachu's Adventure,
forked from MildRain479's PokePark Wiicompiled. No game data is
included: bring your own disc.

Getting started
- Windows 10 or 11, 64-bit, and a graphics card with Vulkan 1.3 or
  OpenGL 3.3.
- Run SparkPark.exe. The first time, it asks for your disc image of
  PokePark Wii: Pikachu's Adventure (USA, R8AE01): an ISO, RVZ, WBFS
  or the like. It checks the image against the original disc, then
  sets the game up in an R8AE01 folder beside the
  program; your saves are kept in R8AE01\nand.
- Keep the prompts and tools folders beside SparkPark.exe.

Settings
- Options are in the pause menu (and on the save files screen):
  display, keyboard, controller and gameplay. They are saved in
  settings.ini beside the program.
- Renderer: Vulkan by default; OpenGL in Options > Display (from the
  next start). If Vulkan can't start, OpenGL is used.
- Xbox-style controllers and keyboard and mouse are supported.
- Updates: at start it asks GitHub whether a newer version is out, and
  if so, Options > Display > Updates opens its download page. Turn the
  Updates setting off to stop it asking.

This is a beta
Some things aren't done yet: a few rare graphics effects, the Wii
Remote speaker, and the Wii Remote pointer and Nunchuk (the game
doesn't need them). It has been tested on one PC so far, so problems
on other hardware are likely.

Reporting problems: open an issue on the project's GitHub page. Say
what you did and what happened, attach a screenshot if you can, and
give your Windows version, graphics card, and the Renderer setting.

Credits
- Based on PokePark Wiicompiled by MildRain479 (CC0):
  https://github.com/MildRain479/PokePark-Wiicompiled
- Button icons by Icons8 (https://icons8.com).
- Disc images are read with decomp-toolkit (tools\dtk.exe); its license
  is in tools\dtk_license.txt.
- PokePark Wii is (c) Nintendo, Creatures Inc., GAME FREAK inc. and The
  Pokemon Company. This project is not affiliated with or endorsed by
  them.
