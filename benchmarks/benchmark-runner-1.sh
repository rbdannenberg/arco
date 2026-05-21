#!/bin/bash

# Configuration
NUM_RUNS=25
WHAT_TO_RUN=Release/fixphase
echo "Running benchmark $WHAT_TO_RUN $NUM_RUNS times..."

# Run benchmarks
for i in $(seq 1 $NUM_RUNS); do
    echo "Run $i of $NUM_RUNS..."

    ${WHAT_TO_RUN}
    sleep 1

done
