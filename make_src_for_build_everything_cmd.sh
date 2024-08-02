#!/bin/sh
# make_src_for_build_everything_cmd.sh - gather sources and make a zip file
#
# Roger B. Dannenberg
# Jun 2024
#
# run this in an empty build directory
# 
# this will zip everything needed for Arco except for these:
#    serpent
#    o2 (but this is built by serpent)
#    wxWidgets
#    faust
#    fluidsynth
# these will be downloaded with curl and built
#
# The zip file that results from this script can be used to compile
# arco from sources as follows:
# 1. create an empty build directory, e.g.
#        cd; mkdir build;
# 2. move arco_src_for_build_everything_cmd.zip to build directory, e.g.
#         mv Downloads/arco_src_for_build_everything_cmd.zip build
# 3. unzip the file, e.g.
#        cd build; unzip arco_src_for_build_everything_cmd.zip
# 4. execute build_everything_cmd.sh in arco, e.g.
#        cd arco; ./build_everything_cmd.sh
#
# The build_everything_cmd.sh file, in building and installing
# serpent, will offer options to configure your system to run
# serpent. See serpent/make_src_for_build_everything_cmd.sh
# for details.
#
# The script will build *only* the test program
# arco/apps/test/daserpent.app. Follow the instructions printed by
# build_everything_cmd.sh to run the daserpent test program.
#

# PRECONDITION: current directory is the build directory that will
# contain arco, serpent, portmidi, etc.

function ask_yes_or_no() {
    read -p "$1 ([y]es or [n]o): "
    case $(echo $REPLY | tr '[A-Z]' '[a-z]') in
        y|yes) echo "yes" ;;
        *)     echo "no" ;;
    esac
}

echo "Warning: creating serpent_src_for_build_everything_cmd.zip will remove"
echo "the serpent directory and you will have to recompile all of it."
if [ "yes" == $(ask_yes_or_no "Create serpent_src_for_build_everything_cmd.zip?") ]
then
  rm -rf serpent
  ~/serpent/make_src_for_build_everything_cmd.sh
fi

echo "Warning: this script will remove arco from this build directory."
echo "You will have to recompile all of it."
if [ "no" == $(ask_yes_or_no "Continue by removing arco?") ]
then
  exit 0
fi

rm -rf arco
mkdir arco
cd arco

mkdir -p apps arco cmupv doc ffts modal pffft preproc serpent server ugens
cd apps
mkdir -p basic common handtrack handtrack/www resound sep test
cd ..
mkdir -p arco/src cmupv/src serpent/src serpent/srp server/src ffts/src
cd modal
mkdir -p docs modal modal/detectionfunctions src
cd ../ugens
mkdir -p lowpass mult notchw reson sine
cd ..
# cwd is build/arco

cp -p ~/arco/README.md .
cp -p ~/arco/build_everything_cmd.sh .
cp -p ~/arco/apps/README.md apps
cp -p ~/arco/apps/basic/*.txt apps/basic/
cp -p ~/arco/apps/basic/dspsources.cmakeinclude apps/basic/
cp -p ~/arco/apps/common/*.{txt,srp,sh} apps/common/
cp -p ~/arco/apps/handtrack/CmakeLists.txt apps/handtrack/
cp -p ~/arco/apps/handtrack/dspmanifest.txt apps/handtrack/
cp -p ~/arco/apps/handtrack/init.srp apps/handtrack/
cp -p ~/arco/apps/handtrack/www/* apps/handtrack/www/
cp -p ~/arco/apps/resound/CmakeLists.txt apps/resound/
cp -p ~/arco/apps/resound/dspmanifest.txt apps/resound/
cp -p ~/arco/apps/resound/*.srp apps/resound/
cp -p ~/arco/apps/sep/CmakeLists.txt apps/sep/
cp -p ~/arco/apps/sep/dspmanifest.txt apps/sep/
cp -p ~/arco/apps/sep/*.{srp,icns} apps/sep/
cp -p ~/arco/apps/test/CmakeLists.txt apps/test/
cp -p ~/arco/apps/test/dspmanifest.txt apps/test/
cp -p ~/arco/apps/test/*.{srp,icns} apps/test/

cp -p ~/arco/arco/CMakeLists.txt arco/
cp -p ~/arco/arco/src/*.{cpp,h,plist,mm} arco/src/

cp -p ~/arco/cmupv/CMakeLists.txt cmupv/
cp -p ~/arco/cmupv/src/*.{c,h,cpp,sal} cmupv/src

cp -p ~/arco/doc/*.{md,txt} doc/

cp -p ~/arco/ffts/src/fftext.h ffts/src/

cp -p ~/arco/modal/LICENSE ~/arco/modal/README.md modal/
cp -p ~/arco/modal/modal/detectionfunctions/*.{cpp,h} \
   modal/modal/detectionfunctions/
cp -p ~/arco/modal/src/*.{cpp,h} modal/src/

cp -p ~/arco/pffft/*.{md,cpp,c,h,hpp} pffft/

cp -p ~/arco/preproc/*.py preproc/

cp -p ~/arco/serpent/arco.cmakeinclude serpent/
cp -p ~/arco/serpent/src/*.{cpp,h} serpent/src/
cp -p ~/arco/serpent/srp/*.srp serpent/srp/

cp -p ~/arco/server/CMakeLists.txt server/
cp -p ~/arco/server/README.md server/
cp -p ~/arco/server/src/*.{cpp,h,c} server/src/

cp -p ~/arco/ugens/README.md ugens/
cp -p ~/arco/ugens/lowpass/lowpass.{cpp,h,srp,ugen} ugens/lowpass
cp -p ~/arco/ugens/lowpass/lowpassb.{cpp,h} ugens/lowpass
cp -p ~/arco/ugens/mult/mult.{cpp,h,srp,ugen} ugens/mult
cp -p ~/arco/ugens/mult/multb.{cpp,h} ugens/mult
cp -p ~/arco/ugens/notchw/notchw.{cpp,h,srp,ugen} ugens/notchw
cp -p ~/arco/ugens/reson/reson.{cpp,h,srp,ugen} ugens/reson
cp -p ~/arco/ugens/reson/resonb.{cpp,h} ugens/reson
cp -p ~/arco/ugens/sine/sine.{cpp,h,srp,ugen} ugens/sine
cp -p ~/arco/ugens/sine/sineb.{cpp,h} ugens/sine

# portaudio sources from nyquist
cp -pR ~/nyquist/portaudio .

# zip everything
cd ..
zip -r arco_src_for_build_everything_cmd.zip arco
rm -rf arco

if [ "yes" == $(ask_yes_or_no "Build arco from scratch here?") ]
then
  unzip arco_src_for_build_everything_cmd.zip
  arco/build_everything_cmd.sh
fi
