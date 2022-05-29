# arco4/ugens directory

These are definitions for arco unit generators represented by
.ugen files.

At the time of writing, there are 3 unit generator types:
sine and mult.

To generate sine unit generators:
```
py ../preproc/u2f.py sine
```
This produces `generate_sine.sh`. Run this to create the
.cpp and .h files that are sources for Arco.

WARNING: these files are brittle: You must be in arco/ugens,
and there must be ../preproc/u2f.py and ../preproc/f2a.py
and ../../o2/preproc/o2idc.py.

For just sine and mult, you can run
```
source generate.sh
```
to do all this in one command.

