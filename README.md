# arco4 - sound synthesis engine and framework

## Roger B. Dannenberg
reorganized April 2022

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
Master CMakeLists.txt is in arco4/apps/*, e.g. see arco4/apps/test/CMakeLists.txt
It includes:
- arco4/serpent/arco4.cmakeinclude, which includes
  - serpent/src/serpent.cmakeinclude to build serpent library
  - arco4/arco/CMakeLists.txt to build arco (mostly dsp) library
  - serpent/64bit/src/serpent to build serpent
  - serpent/wxs/wxs.cmakeinclude to build wxserpent

