#!/bin/sh
py f2a.py mult
cd ../..
py o2idc.py src/faust/mult.cpp
cd src/faust
