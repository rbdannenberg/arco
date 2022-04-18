#!/bin/sh
py f2a.py sine
py f2a.py sineb
cd ../..
py o2idc.py src/faust/sine.cpp
py o2idc.py src/faust/sineb.cpp
cd src/faust
