#!/bin/sh
py f2a.py sine; py f2a.py sineb; py f2a.py mult;
cd ../..
py o2idc.py src/faust/sine.cpp
py o2idc.py src/faust/sineb.cpp
py o2idc.py src/faust/mult.cpp
cd src/faust
