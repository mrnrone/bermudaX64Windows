Bermuda Syndrome x64 README
Release version: 0.0.1
-------------------------------------------------------------------------------

In this fork the code has been updated in order to able to build and run this amazing game on 64-bits Windows in 2023!
During the update I used Windows 10 with Visual Studio 2019.

The following dependencies have been added to the repository:

- 'SDL', 'SDL mixer', 'libvorbis' and 'ogg' have been added via nuget packages. (see packages directory)
- 'libretro' has been added in libretro-common-master directory.
- Some missing header files have been added (by default not available on Windows).

1.How to build?

Run Visual Studio 2019 (or higher)
Open the solution file (it might be necessary to change Windows SDK version and Platform toolset)
Choose your preferred build type debug/release
Build.

2. After successful build

You have to copy from x64\[Debug|Release] diectory the following 3 files into
the root directory of your original game:

bermudax64.exe
SDL2.dll
SDL2_mixer.dll

3. Run the game from the console via the following command:

bermudax64.exe --datapath=.\

4. Save/restore game

It's recommended to check console output which slot is selected (use +/- keys) before 
save or restore :)

-------------------------------------------------------------------------------


Bermuda Syndrome README
Release version: 0.1.7
-------------------------------------------------------------------------------


About:
------

This program is a re-implementation of the engine used in the game Bermuda
Syndrome made by Century Interactive and published by BMG in 1995. The original
data files are required (retail or demo versions).

This rewrite is based on the reversing of the original Windows executable. More
informations about the game can be found at [1] and [2].


Running:
--------

By default, the engine will try to load the game data files from the 'DATA'
directory. The savestates are saved in the current directory. If you have the
CD game soundtrack, you can rip the tracks to 22 Khz stereo Vorbis .ogg files.

The paths can be changed using command line switches :

	Usage: bs [OPTIONS]...
  	--datapath=PATH    Path to data files (default 'DATA')
  	--savepath=PATH    Path to save files (default '.')
	--musicpath=MUSIC  Path to music files (default 'MUSIC')
	--fullscreen       Fullscreen display
	--widescreen=MODE  Widescreen mode ('default', '4:3' or '16:9')

Game hotkeys :

	Arrow Keys      move Jack
	Tab             display bag
	Ctrl            display status bar
	Shift/Space     use weapon
	Enter           skip dialogue speech

Other hotkeys:

	C		capture screenshot as .tga
	S               save game state
	L               load game state
	+               increase game state slot
	-               decrease game state slot
	F               enable fast mode
	W               toggle fullscreen/windowed display

For the Korean version, you need the 'BT16.TBM' file that is installed with
Windows 3.x. The file is 330328 bytes long and should be copied in the same
directory as the 'bs' executable.


Contact:
--------

Gregory Montoir, cyx@users.sourceforge.net


URLs:
-----

[1] http://www.mobygames.com/game/bermuda-syndrome
[2] http://www.reakktor.com/about
