#!/bin/sh
# build_everything.sh - fully automated build arco from sources
#
# Roger B. Dannenberg
# Jun 2024
#

# deployment version for cmake:
OSX_VER=12.0

# option to bypass homebrew and build everything from sources
# set to true to allow linking to brew libraries, set to nothing
# to build from sources (except libsndfile still gets FLAC, Vorbis,
# etc. from libsndfile if they are available.
TRY_BREW=
#TRY_BREW=true

# but we still need some libraries from brew, so make sure they are installed
brew install libogg
brew install libFLAC
brew install libvorbis
brew install libvorbisenc
brew install libvorbisfile
brew install libopus
brew install glib
# libintl is installed by gettext:
brew install gettext

# We need serpent: Get sources from serpent_src_for_build_everything.zip
# and use build_everything.sh
if [ ! -d ../serpent ]
then
  if [ ! -e ../serpent_src_for_build_everything_cmd.zip ]
  then
    echo "================= Downloading Serpent Sources ================="
    curl -L https://sourceforge.net/projects/serpent/files/500/serpent_src_for_build_everything_cmd.zip/download \
         > ../serpent_src_for_build_everything_cmd.zip
  fi
  pushd ..
  unzip serpent_src_for_build_everything_cmd.zip
  cd serpent
  ./build_everything_cmd.sh
  popd
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
if [ ! -d ../fluidsynth ]
then
  if [ ! -e ../fluidsynth.zip ]
  then
    echo "================== Downloading Fluidsynth ======================="
    curl -L https://github.com/FluidSynth/fluidsynth/archive/refs/tags/v$FLSYNVER.zip \
         > ../fluidsynth.zip
  fi  
  pushd ..
  unzip fluidsynth.zip
  mv fluidsynth-$FLSYNVER fluidsynth
  cd fluidsynth
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
  mkdir -p Release
  mv src/libfluidsynth.a Release/libfluidsynth-static.a
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
  mkdir -p Debug
  mv src/libfluidsynth.a Debug/libfluidsynth-static.a
  echo "===== made Debug ===="
  rm src/fluidsynth
  popd
fi

# install PortAudio, but only if it is not there already
# try to use homebrew, then install from sources

if [ -e /opt/homebrew/lib/libportaudio.a ] && [ $TRY_BREW ]
then
  PA_OPT_LIB="/opt/homebrew/lib/libportaudio.a"
  PA_DBG_LIB="/opt/homebrew/lib/libportaudio.a"
  PA_INCL="/opt/homebrew/include"
else
  if [ ! -d ../portaudio ]
  then
    if [ ! -e ../portaudio.tgz ]
    then
      curl -L https://files.portaudio.com/archives/pa_stable_v190700_20210406.tgz \
           > ../portaudio.tgz
    fi
    pushd ..
    tar -xf portaudio.tgz
    cd portaudio
    echo "=============== Building PortAudio Library ==============="
    cmake . -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_OSX_DEPLOYMENT_TARGET=$OSX_VER \
          -DPA_BUILD_SHARED=off -DPA_DISABLE_INSTALL=on
    make clean
    make
    mkdir -p Release
    mv libportaudio.a Release/libportaudio-static.a

    make clean
    cmake . -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=Debug \
          -DCMAKE_OSX_DEPLOYMENT_TARGET=$OSX_VER \
          -DPA_BUILD_SHARED=off -DPA_DISABLE_INSTALL=on
    make 
    mkdir -p Debug
    mv libportaudio.a Debug/libportaudio-static.a
    # back to arco directory:
    popd
  fi
  pushd ../portaudio
  PA_OPT_LIB="$PWD/Release/libportaudio-static.a"
  PA_DBG_LIB="$PWD/Debug/libportaudio-static.a"
  PA_INCL="$PWD/include"
  popd
fi

echo "PA_OPT_LIB = $PA_OPT_LIB"
echo "PA_DBG_LIB = $PA_DBG_LIB"
echo "PA_INCL = $PA_INCL"

if [ -e /opt/homebrew/lib/libsndfile.a ] && [ $TRY_BREW ]
then
  SNDFILE_OPT_LIB="/opt/homebrew/lib/libsndfile.a"
  SNDFILE_DBG_LIB="/opt/homebrew/lib/libsndfile.a"
  SNDFILE_INCL="/opt/homebrew/include"
else
  if [ ! -d ../sndfile ]
  then
    if [ ! -e ../sndfile.zip ]
    then
      curl -L https://github.com/libsndfile/libsndfile/archive/refs/tags/1.2.2.zip \
           > ../sndfile.zip
    fi
    pushd ..
    unzip sndfile.zip
    mv libsndfile-* sndfile
    cd sndfile
    echo "=============== Building SndFile Library ==============="
    # patch homebrew FLAC
    # even if cd fails, we can still pop back to where we were:
    pushd ..
    cd /opt/homebrew/Cellar/flac/*/include/FLAC
    if [ -e assert.h ]
    then
      mv assert.h flacassert.h
    fi        
    popd

    # strangely, the default cc (c compiler) from Xcode doesn't work
    cmake . -DBUILD_EXAMPLES=off -DBUILD_PROGRAMS=off -DBUILD_SHARED_LIBS=OFF \
          -DENABLE_MPEG=OFF \
          -DFLAC_LIBRARY=/opt/homebrew/lib/libFLAC.a \
          -DFLAC_INCLUDE_DIR=/opt/homebrew/include/FLAC \
          -DMP3LAME_LIBRARY=/opt/homebrew/lib/libmp3lame.a \
          -DMP3LAME_INCLUDE_DIR=/opt/homebrew/include/lame \
          -DOGG_LIBRARY=/opt/homebrew/lib/libogg.a \
          -DOGG_INCLUDE_DIR=/opt/homebrew/include \
          -DOPUS_LIBRARY=/opt/homebrew/lib/libopus.a \
          -DOPUS_INCLUDE_DIR=/opt/homebrew/include/opus \
          -DSPEEX_LIBRARY=/opt/homebrew/lib/libspeex.a \
          -DSPEEX_INCLUDE_DIR=/opt/homebrew/include/speex \
          -DVorbis_Enc_LIBRARY=/opt/homebrew/lib/libvorbisenc.a \
          -DVorbis_Enc_INCLUDE_DIR=/opt/homebrew/include/vorbis \
          -DVorbis_File_LIBRARY=/opt/homebrew/lib/libvorbisfile.a \
          -DVorbis_File_INCLUDE_DIR=/opt/homebrew/include/vorbis \
          -DVorbis_Vorbis_LIBRARY=/opt/homebrew/lib/libvorbis.a \
          -DVorbis_Vorbis_INCLUDE_DIR=/opt/homebrew/include/vorbis \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_OSX_DEPLOYMENT_TARGET=$OSX_VER -DBUILD_TESTING=OFF
    make clean
    make
    mkdir -p Release
    mv libsndfile.a Release/libsndfile-static.a

    make clean
    cmake . -DBUILD_EXAMPLES=off -DBUILD_PROGRAMS=off -DBUILD_SHARED_LIBS=OFF \
          -DENABLE_MPEG=OFF \
          -DFLAC_LIBRARY=/opt/homebrew/lib/libFLAC.a \
          -DFLAC_INCLUDE_DIR=/opt/homebrew/include/FLAC \
          -DMP3LAME_LIBRARY=/opt/homebrew/lib/libmp3lame.a \
          -DMP3LAME_INCLUDE_DIR=/opt/homebrew/include/lame \
          -DOGG_LIBRARY=/opt/homebrew/lib/libogg.a \
          -DOGG_INCLUDE_DIR=/opt/homebrew/include \
          -DOPUS_LIBRARY=/opt/homebrew/lib/libopus.a \
          -DOPUS_INCLUDE_DIR=/opt/homebrew/include/opus \
          -DSPEEX_LIBRARY=/opt/homebrew/lib/libspeex.a \
          -DSPEEX_INCLUDE_DIR=/opt/homebrew/include/speex \
          -DVorbis_Enc_LIBRARY=/opt/homebrew/lib/libvorbisenc.a \
          -DVorbis_Enc_INCLUDE_DIR=/opt/homebrew/include/vorbis \
          -DVorbis_File_LIBRARY=/opt/homebrew/lib/libvorbisfile.a \
          -DVorbis_File_INCLUDE_DIR=/opt/homebrew/include/vorbis \
          -DVorbis_Vorbis_LIBRARY=/opt/homebrew/lib/libvorbis.a \
          -DVorbis_Vorbis_INCLUDE_DIR=/opt/homebrew/include/vorbis \
          -DCMAKE_BUILD_TYPE=Debug \
          -DCMAKE_OSX_DEPLOYMENT_TARGET=$OSX_VER -DBUILD_TESTING=OFF
    make 
    mkdir -p Debug
    mv libsndfile.a Debug/libsndfile-static.a

    # unpatch homebrew FLAC
    # even if cd fails, we can still pop back to where we were:
    pushd ..
    cd /opt/homebrew/Cellar/flac/*/include/FLAC
    if [ -e flacassert.h ]
    then
      mv flacassert.h assert.h
    fi        
    popd

    # back to arco directory:
    popd
  fi
  pushd ../sndfile
  SNDFILE_OPT_LIB="$PWD/Release/libsndfile-static.a"
  SNDFILE_DBG_LIB="$PWD/Debug/libsndfile-static.a"
  SNDFILE_INCL="$PWD/include"
  popd
fi

echo "SNDFILE_OPT_LIB = $SNDFILE_OPT_LIB"
echo "SNDFILE_DBG_LIB = $SNDFILE_DBG_LIB"
echo "SNDFILE_INCL = $SNDFILE_INCL"

# O2 is set up by serpent

# Create libraries.txt
BUILD_DIR=$(dirname $PWD)
pushd apps/common
sed "s|/Users/rbd/nyquist/nylsf|$SNDFILE_INCL|g" \
    libraries-example.txt > tmplib1.txt
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
sed "s|/Users/rbd/nyquist/nylsf/ogg|/opt/homebrew/include|g" \
    tmplib2.txt > tmplib1.txt
sed "s|/Users/rbd/nyquist/Release/libogg_static.a|/opt/homebrew/lib/libogg.a|g" \
    tmplib1.txt > tmplib2.txt
sed "s|/Users/rbd/nyquist/Debug/libogg_static.a|/opt/homebrew/lib/libogg.a|g" \
    tmplib2.txt > tmplib1.txt
sed "s|/Users/rbd/nyquist/FLAC/include|/opt/homebrew/include/FLAC|g" \
    tmplib1.txt > tmplib2.txt
sed "s|/Users/rbd/nyquist/Release/libflac_static.a|/opt/homebrew/lib/libFLAC.a|g" \
    tmplib2.txt > tmplib1.txt
sed "s|/Users/rbd/nyquist/Debug/libflac_static.a|/opt/homebrew/lib/libFLAC.a|g" \
    tmplib1.txt > tmplib2.txt
sed "s|/Users/rbd/nyquist/libvorbis/include|/opt/homebrew/include/vorbis|g" \
    tmplib2.txt > tmplib1.txt
sed "s|/Users/rbd/nyquist/Release/libvorbis_static.a|/opt/homebrew/lib/libvorbis.a|g" \
    tmplib1.txt > tmplib2.txt
sed "s|/Users/rbd/nyquist/Debug/libvorbis_static.a|/opt/homebrew/lib/libvorbis.a|g" \
    tmplib2.txt > tmplib1.txt
sed "s|/Users/rbd/nyquist/Release/libvorbisenc_static.a|/opt/homebrew/lib/libvorbisenc.a|g" \
    tmplib1.txt > tmplib2.txt
sed "s|/Users/rbd/nyquist/Debug/libvorbisenc_static.a|/opt/homebrew/lib/libvorbisenc.a|g" \
    tmplib2.txt > tmplib1.txt
sed "s|/Users/rbd/nyquist/Release/libvorbisfile_static.a|/opt/homebrew/lib/libvorbisfile.a|g" \
    tmplib1.txt > tmplib2.txt
sed "s|/Users/rbd/nyquist/Debug/libvorbisfile_static.a|/opt/homebrew/lib/libvorbisfile.a|g" \
    tmplib2.txt > tmplib1.txt
sed "s|/Users/rbd/faust/bin|$HOME/faust/bin|g" tmplib1.txt > tmplib2.txt
sed "s|/Users/rbd/o2/|$BUILD_DIR/o2/|g" tmplib2.txt > tmplib1.txt
sed "s|/Users/rbd/fluidsynth-2.3.3/include|$BUILD_DIR/fluidsynth/include|g" \
    tmplib1.txt > tmplib2.txt
sed "s|/Users/rbd/fluidsynth-2.3.3/build/libfluidsynth-OBJ.build|$BUILD_DIR/fluidsynth|g" \
    tmplib2.txt > tmplib1.txt
# do we need GLIB?
sed "s|/opt/homebrew/Cellar/glib/2.76.4/lib/libglib-2.0.dylib|/opt/homebrew/lib/libglib-2.0.a|g" \
    tmplib1.txt > tmplib2.txt
sed "s|/opt/homebrew/Cellar/gettext/0.21.1/lib/libintl.a|/opt/homebrew/lib/libintl.a|g" tmplib2.txt \
    > tmplib1.txt
sed "s|CMAKE_OSX_DEPLOYMENT_TARGET \"\"|CMAKE_OSX_DEPLOYMENT_TARGET \"$OSX_VER\"|g" tmplib1.txt \
    > tmplib2.txt
mv tmplib2.txt libraries.txt
rm tmplib1.txt
popd
echo "arco/apps/common/libraries.txt is build"

# build the setpath.sh file
pushd ..
echo "export SERPENTPATH=$PWD/serpent/lib:$PWD/serpent/programs:$PWD/serpent/wxslib:$PWD/arco/serpent/srp" \
     > arco/apps/common/setpath.sh
popd

# build the test app
pushd apps/test
cmake . -DCMAKE_BUILD_TYPE=Debug -DUSE_LIBSNDFILE_EXTERNALS=ON \
      -DUSE_GLCANVAS=ON -DUSE_HID=ON -DUSE_MIDI=ON \
      -DUSE_O2=ON -DCMAKE_OSX_DEPLOYMENT_TARGET=$OSX_VER
make
mkdir -p Debug
mv daserpent.app Debug
popd
echo "*---------------------------------"
echo "* Made arco/apps/test"
echo "* You can run it from the arco/apps/test directory using:"
echo "*     cd arco/apps/test"
echo "*     source ../common/setpath.sh"
echo "*     Debug/daserpent.app/Contents/MacOS/daserpent"
echo "*"
echo "* build_everything_cmd.sh - build and installation completed"
echo "*---------------------------------"
