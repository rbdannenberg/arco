#!/bin/sh
# generate_all.sh -- (re)generate all ugens from .ugen files
#
# Roger B. Dannenberg
# Sep 2024
#
# Note: this file may not be up-to-date to generate everything. Normally,
# applications rely on CMake to make what they need.
#
# Assumes "python" will run python3 and faust binaries are on the PATH

echo "(re)generating unit generators: lowpass, mult, notchw, reson, sine"

pushd lowpass
python ../../preproc/u2f.py lowpass
python ../../preproc/f2a.py lowpass
sh generate_lowpass.sh
popd

pushd mult
python ../../preproc/u2f.py mult
python ../../preproc/f2a.py mult
sh generate_mult.sh
popd

pushd notchw
python ../../preproc/u2f.py notchw
python ../../preproc/f2a.py notchw
sh generate_notchw.sh
popd

pushd reson
python ../../preproc/u2f.py reson
python ../../preproc/f2a.py reson
sh generate_reson.sh
popd

pushd sine
python ../../preproc/u2f.py sine
python ../../preproc/f2a.py sine
sh generate_sine.sh
popd


