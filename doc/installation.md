# Arco Installation

**Roger B. Dannenberg**

## Contents

- [Quick Install and Test](quick-install-and-test) - fully automated complete build for MacOS
- [Dependencies](#dependencies) - what software does Arco depend on?
- [General Installation Information](general-installation-information)
- [Installation on MacOS](#installation-on-macos)
- [Installation on Linux](#installation-on-linux)
- [Installation on Windows](#installation-on-windows)
- [CMake Overview](#cMake-overview) - how is the build system organized?

## Quick Install and Test

The zip file `arco_src_for_build_everything_cmd.zip` contains code and a script
that can be used to compile Arco and the daserpent test program as follows:

 1. create an empty build directory, e.g.
    `cd; mkdir build`
 2. move arco_src_for_build_everything_cmd.zip to build directory, e.g.
    `mv Downloads/arco_src_for_build_everything_cmd.zip build`
 3. unzip the file, e.g.
    `cd build; unzip arco_src_for_build_everything_cmd.zip`
 4. execute build_everything_cmd.sh in arco, e.g.
    `cd arco; ./build_everything_cmd.sh`

The build_everything_cmd.sh file, in building and installing serpent,
will offer options to configure your system to run serpent. See
`serpent/make_src_for_build_everything_cmd.sh` for details.

The script will build *only* the test program `arco/apps/test/daserpent.app`.
Follow the instructions printed by `build_everything_cmd.sh` to run the 
`daserpent` test program.

## Dependencies

- SNDFILE_LIB is from libsndfile
- OGGLIB is OGG library used by libsndfile
- FLACLIB is FLAC library used by libsndfile
- VORBIS_LIB and VORBISENC_LIB libraries used by libsndfile
- PORTAUDIO_LIB is PortAudio library
- O2_DBG_LIB and O2_OPT_LIB are O2 library
- WxWidgets is used by wxserpent

## General Installation Information

To build Arco, you need some other programs: O2, PortAudio, PortMidi, and
Serpent. You also need some libraries, described below.

Some things you *have* to build from sources. Others you may be able
to find precompiled and install them. The things you *have* to
install, should all be in the same directory (I use my home directory,
so I have /home/rbd/o2, /home/rbd/portaudio, /home/rbd/portmidi,
/home/rbd/serpent, and /home/rbd/arco.) The directory names `o2`,
`portaudio`, `portmidi`, and `serpent` must be lower case and spelled
the same so that Arco CMake files can find them.

## Installation on MacOS

These instructions assume you want to run Arco as a library within Serpent.
While Arco is under development, this is how we are all running Arco.

- Install CMake (I prefer CMake.app, but you should be able to run ccmake 
from a terminal if you prefer)
- Clone and build o2 from https://github.com/rbdannenberg/o2
- Checkout and build serpent from SourceForge (serpent project:
`https://sourceforge.net/projects/serpent`, see 
`serpent/doc/installation-mac64.htm` for installation guide. This will
take some time since you will need to build WxWidgets from sources. 
  - **Note 1: you need to check out the source code, not a compiled
    application. Do not simply take the MacOS Download, which defaults to
    compiled binaries. You can also find sources (for all platforms) in the
    Linux download or under the Files tab at SourceForge.**
  - **Note 2: Serpent installation instructions begin with installing from
    sources, but you are *building* from sources, so skip down to the
    section on building from osources.**
- Serpent installation instructions (part 1) will show you how to put
  Serpent on your path so you can run it from a Terminal (command line/shell).
  This is optional, but you should at least make sure wxserpent64 runs. Once
  it runs, you will build an Arco app, which will essentially be wxserpent64
  with Arco linked in.
- Clone http://github.com/rbdannenberg/arco. My cloned repo is in `~/arco`,
  but we'll call it ARCODIR
- Install `portaudio`
- Install or build `libsndfile`
- Edit `ARCODIR/apps/common/libraries.txt` (see
  `ARCODIR/apps/common/libraries/libraries-example.txt` for a template) --
  this "localizes" the build to your personal machine. Do not commit
  `libraries.txt` since it is the one part of the build that is specific
  to your machine.
- Run CMake.app and go to `ARCODIR/apps/test` (or in a terminal, you can
  `cd ARCODIR/apps/test; ccmake .`)
- Use configure ('c' in `ccmake`) to process files and offer options to set.
  Then set them as follows:
  - set HAVE_WX_PACKAGE = OFF
  - set USE_GLCANVAS = ON
  - set USE_MIDI = ON
  - set USE_NETWORK = ON
  - set USE_STATIC_LIBS = ON
  - set USE_LIBSNDFILE_EXTERNALS = ON
  - leave OFF: USE_PROC, USE_SHFILE, USE_ZEROMQ, WXS_STDOUT_WINDOW
  - set WX_BASE_PATH to your top wxWidgets path, e.g.
    `/home/rbd/wxWidgets`; there should be wx-build inside this directory
- Use Cmake's configure and generate commands to create an Xcode project
- Build with Xcode (Build "Build-All" and initially, set
  COMMAND-< : Info : Build Configuration to Debug)
- Source `ARCODIR/apps/common/setpath.sh` to set `SERPENTPATH` (must be
  done once unless you switch or restart Terminal) You may have to edit 
  `setpath.sh` to adjust to your local file structure
- run the program from Xcode

## Installation on Windows

These instructions assume you want to run Arco as a library within Serpent. 
While Arco is under development, this is how we are all running Arco.

- If you do not have Visual Studio 2019 Community Edition, install it
  (maybe you can use a later version).
- If you are not set up to run VC tools from the Command Prompt, run this
  is a Command Prompt: 
  `C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build>vcvarsx86_amd64.bat`
  (To test, type "nmake /help" to a command prompt. It should produce
  output, indicating that nmake is now on your path and runnable.)
- Install CMake (I prefer the GUI version. Maybe there is a command line
  version that works.)
- Clone and build o2 from https://github.com/rbdannenberg/o2
- Checkout and build serpent from SourceForge (serpent project:
  `https://sourceforge.net/projects/serpent`, see 
  `serpent/doc/installation-win10.htm` for installation guide. This will
  take some time since you will need to build WxWidgets from sources. 
  - **Note 1: you need to check out the source code, not a compiled
    application. Do not simply take the Windows Download, which defaults to
    compiled binaries. You can also find sources (for all platforms) in the
    Linux download or under the Files tab at SourceForge.**
  - **Note 2: Serpent installation instructions begin with installing from
    sources, but you are *building* from sources, so skip down to the
    section on building from osources.**
- Serpent installation instructions (part 1) will show you how to put
  Serpent on your path so you can run it from a Terminal (command line/shell).
  This is optional, but you should at least make sure wxserpent64 runs. Once
  it runs, you will build an Arco app, which will essentially be wxserpent64
  with Arco linked in.
- Clone http://github.com/rbdannenberg/arco. My cloned repo is in
  `C:/Users/Roger/research/arco`, but we'll call it ARCODIR
- Install `portaudio`
- Install or build `libsndfile`
- Install FluidSynth (I tried `fluidsynth-2.3.5-win10-x64.zip`
  from `https://github.com/FluidSynth/fluidsynth/releases` but I could
  not get it to link. It seems to have all options (and dependencies)
  turned on. Instead, follow 
  `https://github.com/FluidSynth/fluidsynth/wiki/BuildingWithCMake`
  ("Building With Visual Studio on Windows").
  - start with `vcpkg install glib`
  - install the sources. I moved everything from fluidsynth-2.3.5 to just
    ARCODIR/fluidsynth, so CMakeLists.txt is ARCODIR/fluidsynth/CMakeLists.txt
  - modify fluidsynth/CMakeLists.txt by adding this to enable finding packages
    installed with vcpkg:
    `include(/Users/Roger/research/vcpkg/scripts/buildsystems/vcpkg.cmake)`
    (I added this right before `project ( FluidSynth C CXX )`)
  - modify fluidsynth/CMakeLists.txt by adding this. It's as if we don't
    have the right glib .h files because the ones offered by vcpkg create
    external symbols of the form __imp_<name>, and the link fails. I basically
    guessed that maybe this would fix it; I'm not sure what's going on though:
    `add_compile_definitions(GLIB_STATIC_COMPILATION=1)`
    `add_compile_definitions(GOBJECT_STATIC_COMPILATION=1)`
    `add_compile_definitions(GIO_STATIC_COMPILATION=1)`
    (I added these right after `project ( FluidSynth C CXX )`)
  - run CMake on ARCODIR/fluidsynth, with "where to build binaries" in
    ARCODIR/fluidsynth/build. I turned off most options, e.g. enable-aufile is
    OFF. The only ON options (and I'm not sure these are necessary either) are:
    enable-dbus, enable-libinstpatch, enable-libsndfile, enable-openmp,
    enable-threads. In the summary at the end, the "Audio / MIDI driver support"
    is "no" in *all* cases, and every other feature/option is "no" except for
    "Samples type: double" and "Multithread rendering: yes".
  - Open ARCODIR/fluidsynth/build/fluidsynth.sln with Visual Studio
  - Build (at least) libfluidsynth
- Get readline library, e.g. 
  `https://gnuwin32.sourceforge.net/downlinks/readline-bin-zip.php`.
- Extract the files and put them in `ARCODIR/readline/{bin,contrib,
  include,lib,man,manifest}. You actually only need the `lib` files.
- Edit `ARCODIR/apps/common/libraries.txt` (see
  `ARCODIR/apps/common/libraries/libraries-example.txt` for a template) --
  this "localizes" the build to your personal machine. Do not commit
  `libraries.txt` since it is the one part of the build that is specific
  to your machine. Check that FLSYN_INCL, FLSYN_DBG_LIB and FLSYN_OPT_LIB
  match the locations of actual files and directories. Both FLSYN_DBG_LIB
  and FLSYN_OBJ_LIB can have the same value since you probably have only
  one version of the library. My libfluidsynth.lib ended up in
  `/Users/Roger/research/arco/fluidsynth/build/src/Debug/libfluidsynth.lib`
  `fluidsynth.h` appears to be constructed during the build process and
  is therefore in the build directory, so my FLSYN_INCL is set to:
  `/Users/Roger/research/arco/fluidsynth/build/include` Make sure your
  fluidsynth.h is on this path.
  For READLINE_OPT_LIB and READLINE_DBG_LIB,
  the name `readline.lib` seems to be wired into fluidsynth's library,
  so you need to refer specifically to `readline.lib` unless you build
  your own fluidsynth.
- Run CMake.app and go to `ARCODIR/apps/test`
- Use configure to process files and offer options to set. Then set them
  as follows:
  - set HAVE_WX_PACKAGE = OFF
  - set USE_GLCANVAS = ON
  - set USE_MIDI = ON
  - set USE_NETWORK = ON
  - set USE_STATIC_LIBS = ON
  - set USE_LIBSNDFILE_EXTERNALS = ON
  - set USE_HID = OFF
  - leave OFF: USE_PROC, USE_SHFILE, USE_ZEROMQ, WXS_STDOUT_WINDOW
  - set WX_BASE_PATH to your top wxWidgets path, e.g.
    `/home/rbd/wxWidgets`; there should be wx-build inside this directory
- GLIB_DBG_LIB and GLIB_OPT_LIB should be the full path to glib-2.0.lib
- INTL_DBG_LIB should be ignore
- Use Cmake's configure and generate commands to create a Visual Studio
  project (`daserpent.sln`)
- Open `arco/apps/test/daserpent.sln` and build with Visual Studio (start
  with a Debug version)
- Look in `ARCODIR/apps/common/setpath.sh` to find the Unix or MacOS command
  to set `SERPENTPATH`. This does not work on Window, but you should use
  this value for SERPENTPATH in the registry. See
  `serpent/doc/installation-win10.htm` under Installing Compiled Programs
  for notes on setting SERPENTPATH in the registry. (Note that for Arco,
  you need extra directories on the path compared to simply running
  serpent64.exe or wxserpent64.exe.)
- Copy glib-2.0.dll, iconv-2.dll, intl-8.dll, and pcre2-8.dll from 
  xcpkg/installed/x64-windows/bin to the directory with your 
  application, e.g. arco/apps/test/Debug/
- Set search path in the Registry using RegEdit to set
  `Computer\HKEY_LOCAL_MACHINE\SOFTWARE\CMU\Serpent` to
  `SP\lib;SP\programs;SP\wxslib;ARCODIR\serpent\srp`, where SP is the
  serpent directory, e.g. `C:\Users\Roger\research\serpent` and
  ARCODIR is the arco directory, e.g. `C:\Users\Roger\research\arco`
- Download a soundfont, e.g. Google and download FluidR3__GM.sf2.
- Create apps/test/soundfont.srp. It should contain at least:
  `SOUNDFONT = "/Users/Roger/research/arco/fluidsynth/FluidR3_GM/FluidR3__GM.sf2"`
  where the string is actually the full path to a soundfont on your machine.
- Run the compiled Arco program from the Command Prompt.

## Installation on Ubuntu Linux

These instructions are not the only possible installation procedure,
but they describe what I did.

- if you don't have CMake (`ccmake` is the command line program), see
  `https://askubuntu.com/questions/355565/how-do-i-install-the-latest-version-of-cmake-from-the-command-line`
- clone and build o2 from github.com/rbdannenberg/
- clone and build serpent from SourceForge (serpent project:
  https://sourceforge.net/projects/serpent, see serpent/doc/ for
  installation guides -- I have not gotten wxWidgets to work without
  building from sources using the command lines in the guides)
- clone github.com/rbdannenberg/arco. My cloned repo is in
  arco, but we'll call it ARCODIR (see note above).
- install portaudio (on Linux: libportaudio19-dev)
- install Avahi (on Linux: lib-avahi-client-dev)
- install libsndfile (note: on Ubuntu 20, the libsndfile1-dev install will not
     be usable unless you figure out how to link to log_finite and a bunch of
     other symbols. These come from vorbis, a dependency of libsndfile.
     Therefore, I compiled my own.)
  - get v1.1.0 Source code (zip) from
      https://github.com/libsndfile/libsndfile/releases
  - extract to the same directory containing ARCODIR (e.g. your home directory)
  - `mv libsndfile-1.1.0 to libsndfile`
  - `cd libsndfile`
  - `ccmake .`
  - configure (type `c` then `e`)
  - set BUILD_EXAMPLES=OFF, BUILD_PROGRAMS=OFF,CMAKE_BUILD_TYPE=DEBUG
  - set INSTALL_MANPAGES=OFF, ENABLE_EXTERNAL_LIBS=OFF (this eliminates
      the vorbis problem)
  - configure with new settings (type `c`)
  - generate (type `g`) (this should quit cmake and return to shell)
  - `make`
  - you should end up with libsndfile.a in your current directory
- Edit `ARCODIR/apps/common/libraries.txt` (see
  `ARCODIR/apps/common/libraries/libraries-example.txt` for a
  template) -- this "localizes" the build to your personal machine.
  Do not commit `libraries.txt` since it is the one part of the build
  that is specific to your machine.
- cd ARCODIR/apps/test
- ccmake .
- Use ccmake's `c` (configure) command to process files and offer
  options to set. Then set them as follows:
  - set CMAKE_BUILD_TYPE = Debug
  - set HAVE_WX_PACKAGE = OFF
  - set USE_GLCANVAS = ON
  - set USE_MIDI = ON
  - set USE_NETWORK = ON
  - set USE_STATIC_LIBS = ON
  - leave OFF: USE_PROC, USE_SHFILE, USE_ZEROMQ, WXS_STDOUT_WINDOW
  - set WX_BASE_PATH to your top wxWidgets path, e.g. /home/rbd/wxWidgets;
    there should be wx-build inside this directory
- Use ccmake's `c` (configure) and `g` (generate) commands to create a
  Makefile (or Xcode project)
- make (or build with Xcode)
- `source ../common/setpath.sh` to set SERPENTPATH (must be done once
  unless you switch or restart Terminal)
- ./daserpent (on Linux) or Debug/daserpent.app/Contents/MacOS/daserpent
  (on MacOS)
- run the program: `./daserpent` (for "digital audio serpent")
- you should see a list of audio devices in some text like this:
```
O2 message: calling host_openaggr to handle /host/openaggr
0 - ALSA : HDA Intel HDMI: 0 (hw:0,3) (0 ins, 8 outs)
O2 message: calling host_openaggr to handle /host/openaggr
1 - ALSA : HDA Intel HDMI: 1 (hw:0,7) (0 ins, 8 outs)
O2 message: calling host_openaggr to handle /host/openaggr
...
```
and somewhere, a message like: `--> opening this device, id 5`. If not,
find the device you want to open, find a unique substring, and modify
the serpent line in init.srp:
```
    if find(info, "Aggregate") >= 0 or find(info, "Analog") >= 0:
```
As of this writing, this hack wires in a search for the desired device,
which is the Aggregate device on MacOS (so you have to create an
Aggregate device in Audio Midi Setup), or the Analog device on Linux.
You can change either of these, but it's better to just insert
another test case for another string. Ultimately, the plan was to
gather the devices, build a menu, let the user choose a device, and
save the device in preferences.
- You should hear smooth electronic tones with random pitch and random timing.

## CMake Overview

This section describes how CMake and directory structures are organized
to build Arco applications.

This directory with the Arco repo clone can be named anything (I
currently call it arco), so we'll call it `ARCODIR` in the instructions
that follow.

Master CMakeLists.txt is in ARCODIR/apps/*, e.g. 
see ARCODIR/apps/test/CMakeLists.txt which builds a Serpent application
with Arco embedded. It includes:
- ARCODIR/apps/test/CMakeLists.txt
  - ARCODIR/serpent/arco.cmakeinclude, which includes
    - ARCODIR/arco/CMakeLists.txt to build arco (mostly dsp) library
    - serpent/wxs/wxs.cmakeinclude to build wxserpent
      - serpent/src/CMakeLists.txt to build srplib library for wxserpent

Similarly, we have ARCODIR/apps/basic/CMakeLists.txt to build a
stand-alone Arco server. It includes:
- ARCODIR/apps/basic/CMakeLists.txt
  - ARCODIR/server/CMakeLists.txt, which includes
    - ARCODIR/arco/CMakeLists.txt to build arco (mostly dsp) library

To customize the set of unit generators in Arco applications,
`ARCODIR/arco/` contains `dspsources.cmakeinclude` which depends on
`dspmanifest.txt` in the same directory. `dspsources.cmakeinclude`
is created by `makedspmakefile.py`, which also creates `dspmakefile`.
`makedspmakefile.py` is run when CMake generates the project.
To make sure sources are in place, building the project first
runs `make` on `dspmakefile`, which is responsible for creating `.h`
and `.cpp` source files referenced in `dspsources.cmakeinclude`. The
source files are generated by a sequence of scripts, starting with
`u2f.py` to create FAUST programs from Arco's unit generator
description files, then running `f2a.py` to convert FAUST source files
to Arco source files, and finally building O2 interfaces by running
`o2idc.py`.

We build a separate library for each application because
the application includes the entire library (as opposed to including
only the referenced functions. That is because the functions are put
into the O2 method table at runtime when each unit generator runs its
initialization code, which the linker does know about.
