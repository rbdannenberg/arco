echo "In generate_sine.bat running python path=$PATH"
python ../../preproc/f2a.py sine
python ../../preproc/f2a.py sineb
python ../../../o2/preproc/o2idc.py --nobackup sine.cpp
python ../../../o2/preproc/o2idc.py --nobackup sineb.cpp
