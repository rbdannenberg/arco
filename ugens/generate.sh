#!/bin/sh
py ../preproc/u2f.py sine
source generate_sine.sh
py ../preproc/u2f.py mult
source generate_mult.sh
py ../preproc/u2f.py lowpass
source generate_lowpass.sh
