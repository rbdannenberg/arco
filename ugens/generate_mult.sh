#!/bin/sh
py f2a.py mult
py f2a.py multb
cd ../..
py o2idc.py src/faust/mult.cpp
py o2idc.py src/faust/multb.cpp
cd src/faust
