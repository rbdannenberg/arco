#!/bin/sh
# build_everything.sh - fully automated build arco from sources
#
# Roger B. Dannenberg
# Jun 2024
#
# Usage: Normally, you will create an empty build directory (anywhere),
# and unzip arco_src_for_build_everything_cmd.zip from an Arco release
# on github. This will create an arco directory. cd arco and run this
# script there, e.g. simply type ./build_everything_cmd.sh


# deployment version for cmake:
OSX_VER=12.0

# option to bypass homebrew and build everything from sources
# set to true to allow linking to brew libraries, set to nothing
# to build from sources. However, libsndfile may get FLAC, Vorbis,
# etc. from homebrew if they are available there. I haven't
# figured out how to make a completely static link with all the
# libsndfile components (or how to make libsndfile *without* opus,
# ogg, vorbis, mp3, etc.) so the default configuration is to
# simply use homebrew where possible. Update: as of 16 Aug 2024,
# we will build libsndfile locally without mp3 to avoid problems
# encountered using Homebrew's libsndfile installation.
#TRY_BREW=
TRY_BREW=true

# mv_to_new_dir(file, dir) - remove dir, make it, move file to it
function mv_to_new_dir() {
    if [ -e $2 ];
    then
        rm -rf $2
    fi
    mkdir $2
    mv $1 $2
}

# You should start in the build directory containing arco, where sources are
# unzipped

# Test to see if we are running in the arco directory:
if [ `basename $PWD` != "arco" ]
then
  echo "Something is wrong. Current directory should be the build/arco."
  echo "Current directory is: $PWD"
  exit
fi

echo "# At start,running $0 in $PWD"
cd ..
echo "# changing to $PWD"


# but we still need some libraries from brew, so make sure they are installed
brew install libogg
brew install flac
brew install libvorbis
brew install opus
 https://cmu.zoom.us/rec/share/7dxfFLVcdaGEARpGfPAm4Rrp3jLBfKqAWovemNuGQ3mYQvWlXhHXqmQnSJM4ukjD.NtPr1QbMqipBvOH_
Passcode: 22BXvA+J brew install glib
# libintl is installed by gettext:
brew install gettext

# We need serpent: Get sources from serpent_src_for_build_everything.zip
# and use build_everything.sh
if [ ! -d serpent ]
then
  if [ ! -e serpent_src_for_build_everything_cmd.zip ]
  then
    echo "================= Downloading Serpent Sources ================="
    curl -L https://sourceforge.net/projects/serpent/files/500/serpent_src_for_build_everything_cmd.zip/download \
         > serpent_src_for_build_everything_cmd.zip
  fi

  unzip serpent_src_for_build_everything_cmd.zip
  pushd serpent
  echo "# Before build serpent, arco/build_everything_cmd.sh in $PWD"
  echo "====== Building Serpent using serpent/build_everything_cmd.sh  ======"
  ./build_everything_cmd.sh
  popd
  echo "# After build serpent, arco/build_everything_cmd.sh in $PWD"
else
  echo "====== found serpent; assuming Serpent is built and ready  ======"
fi

# building serpent will build o2 and wxWidgets, which we can use for Arco too

# We need faust: get it from dmg and put it in $HOME
FAUSTVER="2.72.14"
if [ ! -d ~/faust ]
then
  if [ ! -e ~/Downloads/Faust-$FAUSTVER.dmg ]
  then
    echo "================== Downloading Faust ======================="
    if [ $(uname -m) == "arm64" ]
    then
      curl -L https://github.com/grame-cncm/faust/releases/download/$FAUSTVER/Faust-$FAUSTVER-arm64.dmg \
           > ~/Downloads/Faust-$FAUSTVER.dmg
    else
      curl -L https://github.com/grame-cncm/faust/releases/download/$FAUSTVER/Faust-$FAUSTVER-x64.dmg \
           > ~/Downloads/Faust-$FAUSTVER.dmg
    fi
  fi
  open ~/Downloads/Faust-$FAUSTVER.dmg
  cp -pR /Volumes/Faust-$FAUSTVER ~/faust
fi

# We need fluidsynth
FLSYNVER="2.3.5"
if [ ! -d fluidsynth ]
then
  if [ ! -e fluidsynth.zip ]
  then
    echo "================== Downloading Fluidsynth ======================="
    curl -L https://github.com/FluidSynth/fluidsynth/archive/refs/tags/v$FLSYNVER.zip \
         > fluidsynth.zip
  fi  
  echo "# Before build fluidsynth, arco/build_everything_cmd.sh in $PWD"
  unzip fluidsynth.zip
  mv fluidsynth-$FLSYNVER fluidsynth
  pushd fluidsynth
  echo "# Before fluidsynth cmake, arco/build_everything_cmd.sh in $PWD"
  cmake . -DBUILD_SHARED_LIBS=off -Denable-pulseaudio=off \
        -Denable-framework=off -Denable-jack=off \
        -Denable-aufile=off -Denable-coreaudio=off \
        -Denable-coremidi=off -Denable-dbus=off \
        -Denable-dsound=off -Denable-ipv6=off \
        -Denable-ladspa=off -Denable-midishare=off \
        -Denable-network=off -Denable-oss=off \
        -Denable-pipewire=off -Denable-readline=off \
        -Denable-sdl2=off -Denable-wasapi=off \
        -Denable-waveout=off -Denable-winmidi=off
  make
  mv src/libfluidsynth.a src/libfluidsynth-static.a
  mv_to_new_dir libfluidsynth-static.a Release
  echo "===== made Release ===="
  rm src/fluidsynth
  make clean
  cmake . -DBUILD_SHARED_LIBS=off -Denable-pulseaudio=off \
        -Denable-framework=off  -Denable-jack=off \
        -Denable-aufile=off -Denable-coreaudio=off \
        -Denable-coremidi=off -Denable-dbus=off \
        -Denable-dsound=off -Denable-ipv6=off \
        -Denable-ladspa=off -Denable-midishare=off \
        -Denable-network=off -Denable-oss=off \
        -Denable-pipewire=off -Denable-readline=off \
        -Denable-sdl2=off -Denable-wasapi=off \
        -Denable-waveout=off -Denable-winmidi=off \
        -DCMAKE_BUILD_TYPE=Debug
  make
  mv src/libfluidsynth.a src/libfluidsynth-static.a
  mv_to_new_dir src/libfluidsynth-static.a Debug
  echo "===== made Debug ===="
  rm src/fluidsynth
  popd
  echo "# After making fluidsynth, arco/build_everything_cmd.sh in $PWD"
fi
# currently we are in build
# install PortAudio, but only if it is not there already
# try to use homebrew, then install from sources

pushd arco/apps/common
HOMEBREW_BASE=$(brew --prefix)
if [ "$HOMEBREW_BASE" == "/opt/homebrew" ]
then
  cp libraries-example.txt tmplib0.txt
else
  sed "s|/opt/homebrew|$HOMEBREW_BASE|g" libraries-example.txt > tmplib0.txt
fi
popd
# now libraries-example.txt has been copied to tmplib0.txt, and if this is
# an Intel machine, the /opt/homebrew paths used by homebrew on ARM systems
# has been rewritten to use /usr/local, used by Intel systems.


if [ -e "$HOMEBREW_BASE/lib/libportaudio.a" ] && [ $TRY_BREW ]
then
  PA_OPT_LIB="$HOMEBREW_BASE/lib/libportaudio.a"
  PA_DBG_LIB="$HOMEBREW_BASE/lib/libportaudio.a"
  PA_INCL="$HOMEBREW_BASE/include"
else
  if [ ! -d portaudio ]
  then
    if [ ! -e portaudio.tgz ]
    then
      curl -L https://files.portaudio.com/archives/pa_stable_v190700_20210406.tgz \
           > portaudio.tgz
    fi
    tar -xf portaudio.tgz
    pushd portaudio
    echo "# Before unzipping portaudio, arco/build_everything_cmd.sh in $PWD"
    echo "# Before building portaudio, arco/build_everything_cmd.sh in $PWD"
    echo "=============== Building PortAudio Library ==============="
    cmake . -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_OSX_DEPLOYMENT_TARGET=$OSX_VER \
          -DPA_BUILD_SHARED=off -DPA_DISABLE_INSTALL=on
    make clean
    make
    mv libportaudio.a libportaudio-static.a
    mv_to_new_dir libportaudio-static.a Release

    make clean
    cmake . -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=Debug \
          -DCMAKE_OSX_DEPLOYMENT_TARGET=$OSX_VER \
          -DPA_BUILD_SHARED=off -DPA_DISABLE_INSTALL=on
    make 
    mv libportaudio.a libportaudio-static.a
    mv_to_new_dir libportaudio-static.a Debug
    # back to arco directory:
    popd
    echo "# After building portaudio, arco/build_everything_cmd.sh in $PWD"
  fi
  pushd portaudio
  echo "# Getting portaudio paths, arco/build_everything_cmd.sh in $PWD"

  PA_DBG_LIB="$PWD/Debug/libportaudio-static.a"
  PA_INCL="$PWD/include"
  popd
  echo "# After portaudio, arco/build_everything_cmd.sh in $PWD"
fi

echo "PA_OPT_LIB = $PA_OPT_LIB"
echo "PA_DBG_LIB = $PA_DBG_LIB"
echo "PA_INCL = $PA_INCL"

# NOTE: This first option did not always work due to unresolved
# dependencies in Homebrew's libsndfile, so we always build it
# locally now.
#if [ -e $HOMEBREW_BASE/lib/libsndfile.a ] && [ $TRY_BREW ]
#then
#  SNDFILE_OPT_LIB="$HOMEBREW_BASE/lib/libsndfile.a"
#  SNDFILE_DBG_LIB="$HOMEBREW_BASE/lib/libsndfile.a"
#  SNDFILE_INCL="$HOMEBREW_BASE/include"
#else
  if [ ! -d sndfile ]
  then
    if [ ! -e sndfile.zip ]
    then
      curl -L https://github.com/libsndfile/libsndfile/archive/refs/tags/1.2.2.zip \
           > sndfile.zip
    fi
    # ready to pop back to arco, we are in build
    unzip sndfile.zip
    mv libsndfile-* sndfile
    pushd sndfile
    echo "# Before building libsndfile, arco/build_everything_cmd.sh in $PWD"
    echo "=============== Building SndFile Library ==============="
    # patch homebrew FLAC
    # even if cd fails, we can still pop back to where we were:
    pushd ..
    cd $HOMEBREW_BASE/Cellar/flac/*/include/FLAC
    echo "# Before patching FLAC, arco/build_everything_cmd.sh in $PWD"
    if [ -e assert.h ]
    then
      mv assert.h flacassert.h
    fi        
    popd
    echo "# After patching FLAC, arco/build_everything_cmd.sh in $PWD"
    # strangely, the default cc (c compiler) from Xcode doesn't work
    cmake . -DBUILD_EXAMPLES=off -DBUILD_PROGRAMS=off -DBUILD_SHARED_LIBS=OFF \
          -DENABLE_MPEG=OFF \
          -DFLAC_LIBRARY=$HOMEBREW_BASE/lib/libFLAC.a \
          -DFLAC_INCLUDE_DIR=$HOMEBREW_BASE/include/FLAC \
          -DMP3LAME_LIBRARY=$HOMEBREW_BASE/lib/libmp3lame.a \
          -DMP3LAME_INCLUDE_DIR=$HOMEBREW_BASE/include/lame \
          -DOGG_LIBRARY=$HOMEBREW_BASE/lib/libogg.a \
          -DOGG_INCLUDE_DIR=$HOMEBREW_BASE/include \
          -DOPUS_LIBRARY=$HOMEBREW_BASE/lib/libopus.a \
          -DOPUS_INCLUDE_DIR=$HOMEBREW_BASE/include/opus \
          -DSPEEX_LIBRARY=$HOMEBREW_BASE/lib/libspeex.a \
          -DSPEEX_INCLUDE_DIR=$HOMEBREW_BASE/include/speex \
          -DVorbis_Enc_LIBRARY=$HOMEBREW_BASE/lib/libvorbisenc.a \
          -DVorbis_Enc_INCLUDE_DIR=$HOMEBREW_BASE/include/vorbis \
          -DVorbis_File_LIBRARY=$HOMEBREW_BASE/lib/libvorbisfile.a \
          -DVorbis_File_INCLUDE_DIR=$HOMEBREW_BASE/include/vorbis \
          -DVorbis_Vorbis_LIBRARY=$HOMEBREW_BASE/lib/libvorbis.a \
          -DVorbis_Vorbis_INCLUDE_DIR=$HOMEBREW_BASE/include/vorbis \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_OSX_DEPLOYMENT_TARGET=$OSX_VER -DBUILD_TESTING=OFF
    make clean
    make
    mv libsndfile.a libsndfile-static.a
    mv_to_new_dir libsndfile-static.a Release

    make clean
    cmake . -DBUILD_EXAMPLES=off -DBUILD_PROGRAMS=off -DBUILD_SHARED_LIBS=OFF \
          -DENABLE_MPEG=OFF \
          -DFLAC_LIBRARY=$HOMEBREW_BASE/lib/libFLAC.a \
          -DFLAC_INCLUDE_DIR=$HOMEBREW_BASE/include/FLAC \
          -DMP3LAME_LIBRARY=$HOMEBREW_BASE/lib/libmp3lame.a \
          -DMP3LAME_INCLUDE_DIR=$HOMEBREW_BASE/include/lame \
          -DOGG_LIBRARY=$HOMEBREW_BASE/lib/libogg.a \
          -DOGG_INCLUDE_DIR=$HOMEBREW_BASE/include \
          -DOPUS_LIBRARY=$HOMEBREW_BASE/lib/libopus.a \
          -DOPUS_INCLUDE_DIR=$HOMEBREW_BASE/include/opus \
          -DSPEEX_LIBRARY=$HOMEBREW_BASE/lib/libspeex.a \
          -DSPEEX_INCLUDE_DIR=$HOMEBREW_BASE/include/speex \
          -DVorbis_Enc_LIBRARY=$HOMEBREW_BASE/lib/libvorbisenc.a \
          -DVorbis_Enc_INCLUDE_DIR=$HOMEBREW_BASE/include/vorbis \
          -DVorbis_File_LIBRARY=$HOMEBREW_BASE/lib/libvorbisfile.a \
          -DVorbis_File_INCLUDE_DIR=$HOMEBREW_BASE/include/vorbis \
          -DVorbis_Vorbis_LIBRARY=$HOMEBREW_BASE/lib/libvorbis.a \
          -DVorbis_Vorbis_INCLUDE_DIR=$HOMEBREW_BASE/include/vorbis \
          -DCMAKE_BUILD_TYPE=Debug \
          -DCMAKE_OSX_DEPLOYMENT_TARGET=$OSX_VER -DBUILD_TESTING=OFF
    make 
    mkdir -p Debug
    mv libsndfile.a libsndfile-static.a
    mv_to_new_dir libsndfile-static.a Debug

    # unpatch homebrew FLAC
    # prepare to pop back to here, which is build/sndfile
    pushd ..
    echo "# After building libsndfile, arco/build_everything_cmd.sh in $PWD"
    cd $HOMEBREW_BASE/Cellar/flac/*/include/FLAC
    echo "# Ready to restore FLAC, arco/build_everything_cmd.sh in $PWD"
    if [ -e flacassert.h ]
    then
      mv flacassert.h assert.h
    fi        
    popd
    echo "# Done restoring FLAC, arco/build_everything_cmd.sh in $PWD"

    # back to build directory:
    popd
    echo "# Back to build, arco/build_everything_cmd.sh in $PWD"
  fi
  pushd sndfile
  echo "# Setting variables for built sndfile, arco/build_everything_cmd.sh in $PWD"
  SNDFILE_OPT_LIB="$PWD/Release/libsndfile-static.a"
  SNDFILE_DBG_LIB="$PWD/Debug/libsndfile-static.a"
  SNDFILE_INCL="$PWD/include"
  popd
  echo "# Completed work on sndfile, arco/build_everything_cmd.sh in $PWD"
#fi

echo "SNDFILE_OPT_LIB = $SNDFILE_OPT_LIB"
echo "SNDFILE_DBG_LIB = $SNDFILE_DBG_LIB"
echo "SNDFILE_INCL = $SNDFILE_INCL"

# O2 is set up by serpent

# Create libraries.txt
BUILD_DIR=$PWD
pushd arco/apps/common
echo "# Start libraries.txt, arco/build_everything_cmd.sh in $PWD"
sed "s|/Users/rbd/nyquist/nylsf|$SNDFILE_INCL|g" \
    tmplib0.txt > tmplib1.txt
sed "s|/Users/rbd/nyquist/Release/libsndfile_static.a|$SNDFILE_OPT_LIB|g" \
    tmplib1.txt > tmplib2.txt
sed "s|/Users/rbd/nyquist/Debug/libsndfile_static.a|$SNDFILE_DBG_LIB|g" \
    tmplib2.txt > tmplib1.txt
sed "s|/Users/rbd/nyquist/portaudio/include|$PA_INCL|g" \
    tmplib1.txt > tmplib2.txt
sed "s|/Users/rbd/nyquist/Release/libportaudio_static.a|$PA_OPT_LIB|g" \
    tmplib2.txt > tmplib1.txt
sed "s|/Users/rbd/nyquist/Debug/libportaudio_static.a|$PA_DBG_LIB|g" \
    tmplib1.txt > tmplib2.txt
sed "s|/Users/rbd/nyquist/nylsf/ogg|$HOMEBREW_BASE/include|g" \
    tmplib2.txt > tmplib1.txt
sed "s|/Users/rbd/nyquist/Release/libogg_static.a|$HOMEBREW_BASE/lib/libogg.a|g" \
    tmplib1.txt > tmplib2.txt
sed "s|/Users/rbd/nyquist/Debug/libogg_static.a|$HOMEBREW_BASE/lib/libogg.a|g" \
    tmplib2.txt > tmplib1.txt
sed "s|/Users/rbd/nyquist/FLAC/include|$HOMEBREW_BASE/include/FLAC|g" \
    tmplib1.txt > tmplib2.txt
sed "s|/Users/rbd/nyquist/Release/libflac_static.a|$HOMEBREW_BASE/lib/libFLAC.a|g" \
    tmplib2.txt > tmplib1.txt
sed "s|/Users/rbd/nyquist/Debug/libflac_static.a|$HOMEBREW_BASE/lib/libFLAC.a|g" \
    tmplib1.txt > tmplib2.txt
sed "s|/Users/rbd/nyquist/libvorbis/include|$HOMEBREW_BASE/include/vorbis|g" \
    tmplib2.txt > tmplib1.txt
sed "s|/Users/rbd/nyquist/Release/libvorbis_static.a|$HOMEBREW_BASE/lib/libvorbis.a|g" \
    tmplib1.txt > tmplib2.txt
sed "s|/Users/rbd/nyquist/Debug/libvorbis_static.a|$HOMEBREW_BASE/lib/libvorbis.a|g" \
    tmplib2.txt > tmplib1.txt
sed "s|/Users/rbd/nyquist/Release/libvorbisenc_static.a|$HOMEBREW_BASE/lib/libvorbisenc.a|g" \
    tmplib1.txt > tmplib2.txt
sed "s|/Users/rbd/nyquist/Debug/libvorbisenc_static.a|$HOMEBREW_BASE/lib/libvorbisenc.a|g" \
    tmplib2.txt > tmplib1.txt
sed "s|/Users/rbd/nyquist/Release/libvorbisfile_static.a|$HOMEBREW_BASE/lib/libvorbisfile.a|g" \
    tmplib1.txt > tmplib2.txt
sed "s|/Users/rbd/nyquist/Debug/libvorbisfile_static.a|$HOMEBREW_BASE/lib/libvorbisfile.a|g" \
    tmplib2.txt > tmplib1.txt
sed "s|/Users/rbd/faust/bin|$HOME/faust/bin|g" tmplib1.txt > tmplib2.txt
sed "s|/Users/rbd/o2/|$BUILD_DIR/o2/|g" tmplib2.txt > tmplib1.txt
sed "s|/Users/rbd/fluidsynth-2.3.3/include|$BUILD_DIR/fluidsynth/include|g" \
    tmplib1.txt > tmplib2.txt
sed "s|/Users/rbd/fluidsynth-2.3.3/build/libfluidsynth-OBJ.build|$BUILD_DIR/fluidsynth|g" \
    tmplib2.txt > tmplib1.txt
# do we need GLIB?
sed "s|$HOMEBREW_BASE/Cellar/glib/2.76.4/lib/libglib-2.0.dylib|$HOMEBREW_BASE/lib/libglib-2.0.a|g" \
    tmplib1.txt > tmplib2.txt
sed "s|$HOMEBREW_BASE/Cellar/gettext/0.21.1/lib/libintl.a|$HOMEBREW_BASE/lib/libintl.a|g" tmplib2.txt \
    > tmplib1.txt
sed "s|CMAKE_OSX_DEPLOYMENT_TARGET \"\"|CMAKE_OSX_DEPLOYMENT_TARGET \"$OSX_VER\"|g" tmplib1.txt \
    > tmplib2.txt
mv tmplib2.txt libraries.txt
rm tmplib1.txt
popd
echo "# After arco/apps/common/libraries.txt is built, $PWD"

# build the setpath.sh file
echo "# Create setpath.sh (SERPENTPATH), arco/build_everything_cmd.sh in $PWD"
echo "export SERPENTPATH=$PWD/serpent/lib:$PWD/serpent/programs:$PWD/serpent/wxslib:$PWD/arco/serpent/srp" \
     > arco/apps/common/setpath.sh
echo "# After making setpath.sh, arco/build_everything_cmd.sh in $PWD"


# build the test app
pushd arco/apps/test
echo "# Building apps/test, arco/build_everything_cmd.sh in $PWD"
cmake . -DCMAKE_BUILD_TYPE=Debug -DUSE_LIBSNDFILE_EXTERNALS=ON \
      -DUSE_GLCANVAS=ON -DUSE_HID=ON -DUSE_MIDI=ON \
      -DUSE_O2=ON -DCMAKE_OSX_DEPLOYMENT_TARGET=$OSX_VER
make
mv_to_new_dir daserpent.app Debug
popd

echo "# After running $0 in $PWD and building apps/test, "
echo "*---------------------------------"
echo "* Made arco/apps/test in $PWD"
echo "* You can run it from the $PWD/arco/apps/test directory using:"
echo "*     cd $PWD/arco/apps/test"
echo "*     source ../common/setpath.sh"
echo "*     Debug/daserpent.app/Contents/MacOS/daserpent"
echo "*"
echo "* $0 - build and installation completed"
echo "*---------------------------------"
