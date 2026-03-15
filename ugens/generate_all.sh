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

pushd highpass
python ../../preproc/u2f.py highpass
python ../../preproc/f2a.py highpass
sh generate_highpass.sh
popd

pushd mult
python ../../preproc/u2f.py mult
python ../../preproc/f2a.py mult
sh generate_mult.sh
popd

pushd notch
python ../../preproc/u2f.py notch
python ../../preproc/f2a.py notch
sh generate_notch.sh
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

pushd noisegate
python ../../preproc/u2f.py noisegate
python ../../preproc/f2a.py noisegate
sh generate_noisegate.sh
popd

pushd stnoisegate
python ../../preproc/u2f.py stnoisegate
python ../../preproc/f2a.py stnoisegate
sh generate_stnoisegate.sh
popd

pushd zsine
python ../../preproc/u2f.py zsine
python ../../preproc/f2a.py zsine
sh generate_zsine.sh
popd

pushd sttest
python ../../preproc/u2f.py sttest
python ../../preproc/f2a.py sttest
sh generate_sttest.sh
popd

pushd overdrive
python ../../preproc/u2f.py overdrive
python ../../preproc/f2a.py overdrive
sh generate_overdrive.sh
popd

pushd zitarev
python ../../preproc/u2f.py zitarev
python ../../preproc/f2a.py zitarev
sh generate_zitarev.sh
popd
