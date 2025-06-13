#!/bin/bash

# Configuration
NUM_RUNS=25

echo "Running benchmarks $NUM_RUNS times..."

# Run benchmarks

for i in $(seq 1 $NUM_RUNS); do
    echo "Run $i of $NUM_RUNS..."
    Release/singlechannel
    sleep 1
#    Release/fastchannels
#    sleep 1
#    Release/noterminate
#    sleep 1
#    Release/nopoly
#    sleep 1
#    Release/singleugen
#    sleep 1
#    Release/noclosure
#    sleep 1
#    Release/block32
#    sleep 1
#    Release/block64
#    sleep 1
#    Release/block2
#    sleep 1
#    Release/block4
#    sleep 1
#    Release/block8
#    sleep 1
#    Release/block16
#    sleep 1
#    Release/block128
#    sleep 1
#    Release/block256
#    sleep 1
    Release/arco32
    sleep 1
#    Release/arco64
#    sleep 1
#    Release/arco2
#    sleep 1
#    Release/arco4
#    sleep 1
#    Release/arco8
#    sleep 1
#    Release/arco16
#    sleep 1
#    Release/arco128
#    sleep 1
#    Release/arco256
#    sleep 1
    Release/fixphase
    sleep 1
#    Release/arcolike2
#    sleep 1
    Release/arcoopt
    sleep 1
done
exit

