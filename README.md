# arco4 - sound synthesis engine and framework

## Roger B. Dannenberg
reorganized April 2022, linux port May 2022

See design.md for technical design discussions/plans

## Contents
- Directories
- Dependencies
- CMake Overview
- Installation on Ubuntu Linux

## Directories
- apps -- applications where you can build customized versions of Arco servers with or without Serpent.
- arco -- the Arco main synthesis engine code
- preproc -- preprocessors written in Python to translate Arco's .ugen files, which add some annotations to FAUST source, into to FAUST .dsp files. Also runs FAUST, manipulates the output to create Arco unit generators.
- serpent -- shared code for building Serpent interpreter combined with Arco synthesis engine running in separate thread with an O2 shared memory interface.
- server -- builds a command-line application acting as an O2 server with
Arco synthesis engine running in a separater thread with an O2 shared memory interface. This should really just contain the shared code, and actual server instances should be built in the apps directory so that they can have custom code.
- ugens -- Arco unit generators implemented in .ugen files and translated by a combination of FAUST and preprocessors to Arco unit generators.

## Dependencies
- SNDFILE_LIB is from libsndfile
- OGGLIB is OGG library used by libsndfile
- FLACLIB is FLAC library used by libsndfile
- VORBIS_LIB and VORBISENC_LIB libraries used by libsndfile
- PORTAUDIO_LIB is PortAudio library
- O2_DBG_LIB and O2_OPT_LIB are O2 library
- WxWidgets is used by wxserpent

## CMake Overview

To build Arco, you need some other programs: O2, PortAudio, PortMidi, and
Serpent. You also need some libraries, described below.

Some things you *have* to build from sources. Others you may be able
to find precompiled and install them. The things you *have* to
install, should all be in the same directory (I use my home directory,
so I have /home/rbd/o2, /home/rbd/portaudio, /home/rbd/portmidi,
/home/rbd/serpent, and /home/rbd/arco4.) The directory names `o2`,
`portaudio`, `portmidi`, and `serpent` must be lower case and spelled
the same so that Arco CMake files can find them.

This directory with the Arco repo clone can be named anything (I
currently call it arco4), so we'll call it `ARCODIR` in the instructions
that follow. Of course, use your top-level directory name instead of
`ARCODIR`.


Master CMakeLists.txt is in ARCODIR/apps/*, e.g. see ARCODIR/apps/test/CMakeLists.txt
It includes:
- ARCODIR/apps/test/CMakeLists.txt
  - ARCODIR/serpent/arco4.cmakeinclude, which includes
    - ARCODIR/arco/CMakeLists.txt to build arco (mostly dsp) library
    - serpent/wxs/wxs.cmakeinclude to build wxserpent
      - serpent/src/CMakeLists.txt to build srplib library for wxserpent

## Installation on Ubuntu Linux

These instructions are not the only possible installation procedure,
but they describe what I did.

- if you don't have CMake (`ccmake` is the command line program), see https://askubuntu.com/questions/355565/how-do-i-install-the-latest-version-of-cmake-from-the-command-line
- clone and build serpent from SourceForge (serpent project: https://sourceforge.net/projects/serpent, see serpent/doc/ for installation guides -- I have not gotten wxWidgets to work without building from sources using the command lines in the guides)
- clone and build o2 from github.com/rbdannenberg/
- clone github.com/rbdannenberg/arco. My cloned repo is in
  arco4, but we'll call it ARCODIR (see note above).
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
  - set WX_BASE_PATH to your top wxWidgets path, e.g. /home/rbd/wxWidgets; there should be wx-build inside this directory
- Use ccmake's `c` (configure) and `g` (generate) commands to create a Makefile (or Xcode project)
- make (or build with Xcode)
- `source ../common/setpath.sh` to set SERPENTPATH (must be done once unless you switch or restart Terminal)
- ./daserpent (on Linux) or Debug/daserpent.app/Contents/MacOS/daserpent (on MacOS)
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
