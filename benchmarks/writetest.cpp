// writetest.cpp -- experiment to study timing
//
// Roger B. Dannenberg
// May 2025
//
// The optimal block size appears to be 8 (see timing for block2,
// block4, ..., block256.cpp). Why? This "microbenchmark" proposes
// that when a ugen computes very quickly, the limiting factor is
// write speed. Furthermore, it hypothesizes that some initial
// writes are absorbed quickly by write buffers hidden in the core,
// so if there are small blocks (size 8?), and there is non-write
// behavior following the writes, there is time to complete the
// writes in parallel with other computation. But if buffers are
// large, the write buffers are unable to absorb all the writes
// and the CPU stalls until writes complete.
//
// If this works, maybe it is because *all* cases with small buffers
// run faster. I can't see why, but we should create a "control" for
// the experiment where both inner loops are the same and produce a
// lot of writes.
//
// We will not try to prove this explanation is correct, but we
// can at least try to falsify it with a simple direct experiment.
// If the experiment does not falsify the hypothesis, at least
// we have a plausible explanation.
//
// The experiment alternately writes a block of samples computed
// as a simple linear ramp, then reads the block and performs
// something much slower but with few writes by doing math operations.
// We will time this with different block sizes.
//


#include <unistd.h>
#include <math.h>
#include <stdio.h>
#include "o2.h"

#define BIG 128
#define SMALL 4
#define RUNS 10
#define SAMPS 1000000000

float buffer[BIG];
float buffer2[BIG];


O2time run_test(int n)
{
    O2time start_time = o2_local_time();
    long nblocks = SAMPS / n;
    float amp = 0.0;
    float total = 0.0;
    for (int blocks = 0; blocks < nblocks; blocks++) {
        // first "ugen" computes ramp that resets to zero when it reaches 1
        for (int i = 0; i < n; i++) {
            buffer[i] = amp;
            amp += 0.000001;
            if (amp >= 1) {
                amp = 0;
            }
        }
        // second "ugen" does more computation with the input samples
        for (int i = 0; i < n; i++) {
            float out = buffer[i];
            out = sin(out) + cos(out);
            if (out < 0) {
                out = -out;
            }
            // we won't even write anything here, but we'll use the results
            // to make sure the compiler does not optimize out a dead variable
            total += sqrt(out);
        }
    }
    O2time finish_time = o2_local_time();
    printf("%g ", total);  // just to make sure total gets used
    return finish_time - start_time;
}


// similar to run_test, but this is the "control" where both ugen loops
// do the same, and under our hypothesis keep the write buffer full. We
// predict large blocks will perform the same or maybe just slightly
// better than small blocks. (Large blocks do a tiny bit less computation
// but the rate-limiting operation should be memory writes and both write
// the same number of samples.)
//
O2time run_control(int n)
{
    O2time start_time = o2_local_time();
    long nblocks = SAMPS / n;
    float amp = 0.0;
    for (int blocks = 0; blocks < nblocks; blocks++) {
        // first "ugen" computes ramp that resets to zero when it reaches 1
        for (int i = 0; i < n; i++) {
            buffer[i] = amp;
            amp += 0.000001;
            if (amp >= 1) {
                amp = 0;
            }
        }
        // second "ugen" copies input to output
        for (int i = 0; i < n; i++) {
            buffer[i] = amp;
            amp += 0.000001;
            if (amp >= 1) {
                amp = 0;
            }
        }
    }
    O2time finish_time = o2_local_time();
    return finish_time - start_time;
}
    

int main()
{
    o2_internet_enable(false);
    o2_initialize("arcobenchmark");
    o2_clock_set(NULL, NULL);

    O2time small = 0.0;
    O2time big = 0.0;
    O2time smallctrl = 0.0;
    O2time bigctrl = 0.0;

    for (int i = 0; i < RUNS; i++) {
        O2time sc = run_control(SMALL);
        smallctrl += sc;
        usleep(20000);  // 20ms
        O2time bc = run_control(BIG);
        bigctrl += bc;
        usleep(20000);  // 20ms
        O2time s = run_test(SMALL);
        small += s;
        usleep(20000);  // 20ms
        O2time b = run_test(BIG);
        big += b;
        printf("%d: smallctrl %g bigctrl %g small %g big %g\n", i, sc, bc, s, b);
        usleep(20000);  // 20ms
    }
    printf("Average over %d runs: smallctrl %g bigctrl %g small %g big %g\n",
           RUNS, smallctrl / RUNS, bigctrl / RUNS, small / RUNS, big / RUNS);
    printf("Experimental time ratio (big/small): %g\n"
           "Control speed ratio (big/small): %g\n",
           big / small, bigctrl / smallctrl);
    return 0;
}
