#!/bin/bash

# Configuration
NUM_RUNS=25

echo "Running benchmarks $NUM_RUNS times..."

# Run benchmarks
for i in $(seq 1 $NUM_RUNS); do
    echo "Run $i of $NUM_RUNS..."

    Release/arcolike
    sleep 1
    Release/singlechannel
    sleep 1
    Release/fastchannels
    sleep 1
    Release/noterminate
    sleep 1
    Release/nopoly
    sleep 1
    Release/allaudio
    sleep 1
    Release/block64
    sleep 1

done

