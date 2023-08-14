/* resamptest.cpp -- evaluate resampling to help with parameter selection
 *
 * Roger B. Dannenberg
 */

#include <cmath>
#include "arcougen.h"
// this is ugly: I want arcougen.h included because it defines AR (audio rate),
// Vec (my version of dynamic arrays, defined in O2), etc., but arcougen.h
// assumes you are a "sharedmemclient" and therefore cannot be a main O2
// process. But we have to be an O2 process to call o2_initialize(). I would
// prefer to just use o2mem (for memory allocation, needed by Vec), but o2mem
// depends on o2_ctx (per-thread context needed for lock-free memory
// allocation).  So it's complicated. The hack here is that sharedmemclient.h
// defines o2_initialize() to prevent us from calling it, but we'll undefine
// it to expose it to our main() and let us initialize O2:
#undef o2_initialize

#include "stdio.h"
#include "resamp.h"

#define OUTLEN 88200
// input is extra long in case we are downsampling and need more input
#define INLEN (OUTLEN * 2 + 200)  // cushion for span and rounding

Sample input[INLEN];
Sample output[OUTLEN];


void sine_fill(double freq) {
    for (int i = 0; i < INLEN; i++) {
        input[i] = sin(2 * M_PI * freq * i / AR);
    }
}


void sine_sweep(double starthz, double endhz, double dur)
{
    double phase = 0.0;
    for (int i = 0; i < INLEN; i++) {
        double time = i * AP;
        double hz = starthz + (endhz - starthz) * (time / dur);
        phase += 2 * M_PI * hz / AR;
        if (phase > 2 * M_PI) {
            phase -= 2 * M_PI;
        }
        input[i] = sin(phase);
    }
}


void output_sine(double freq) {
    sine_fill(freq);
    for (int i = 0; i < OUTLEN; i++) {
        output[i] = input[i];
    }
}


void write_signal(const char *filename,
                  Sample_ptr signal, int starti, int endi) {
    FILE *sf = fopen(filename, "w");
    for (int i = starti; i < endi; i++) {
        fprintf(sf, "%g\n", signal[i]);
    }
    fclose(sf);
}


void write_output(const char *filename) {
    write_signal(filename, output, 0, OUTLEN);
}


int main(int argc, const char *argv[])
{
    o2_initialize("resamptest");
    if (argc != 4) {
        printf("resampletest span oversample mode\n");
        printf("    mode 0 - basic\n");
        printf("    mode 1 - distortion baseline 1kHz, 1->1.1kHz, 1->0.9kHz\n");
        printf("    mode 2 - aliasing test\n");
        printf("    mode 3 - sweep and resample\n");
        printf("    mode 4 - execution time\n");
        return 1;
    }
    int span = atoi(argv[1]);
    int oversample = atoi(argv[2]);

    Resamp resamp(span, oversample);

    if (strcmp(argv[3], "0") == 0) {  // basic (sanity check)

        for (int i = 0; i < span * oversample / 2; i++) {
            output[i] = resamp.sinc[i].sinc;
        }
        write_signal("wsinc.dat", output, 0, span * oversample / 2);

        for (int i = 0; i < span * oversample / 2; i++) {
            output[i] = resamp.sinc[i].dsinc;
        }
        write_signal("dsinc.dat", output, 0, span * oversample / 2);

        // Simple resample test - sanity check -- resample 1Khz to 1.1Khz
        sine_fill(1000.0);
        // this conversion needs to reconstruct the signal, so scale is 1
        // offset needed: use span, which is bigger then necessary.
        resamp.resamp(output, OUTLEN, input, span, 1.0, 1.0 / 1.1);
        write_output("d0.dat");
    }

    if (strcmp(argv[3], "1") == 0) {  // distortion
        const double ratio = 1.0 / 0.9111;

        // baseline for 1kHz
        output_sine(1000.0);
        write_output("d1a.dat");
    
        sine_fill(1000.0);
        resamp.resamp(output, OUTLEN, input, span, 1.0, 1.0 / ratio);
        write_output("d1b.dat");

        sine_fill(1000.0);
        resamp.resamp(output, OUTLEN, input, span, ratio, ratio);
        write_output("d1c.dat");

        output_sine(1000.0 * ratio);
        write_output("d1d.dat");

#ifdef NEAREXACT
        // let's find out what kind of error we get from very accurate
        // reconstruction
        sine_fill(1000.0);
        int hspan = 10;   // compute sinc from output -hspan to +hspan
        double scale = ratio;
        double inhspan = hspan * scale;
        for (int i = 0; i < OUTLEN; i++) {
            // time is input time, where input has sample rate = 1
            double time = i * scale;  // where to interpolate input
            double sum = 0;
            if (i > hspan) {
                int istart = std::ceil(time - inhspan);
                int iend = std::floor(time + inhspan);
                for (int j = istart; j <= iend; j++) {
                    // j is at lower sample rate, so time at j is j / scale:
                    // dt goes from -inhspan to +inhspan, so raised cos
                    // must have one cycle, with phase from 0 to 2PI, so
                    // phase is (1 + dt / inhspan) * PI
                    double dt = j - time;
                    double x = M_PI * dt / scale;
                    double sinc = 1.0;
                    if (x != 0) sinc = sin(x) / x;
                    double w = 0.42 - 0.5 * cos(M_PI * (1 + dt / inhspan)) +
                               0.08 * cos(2 * M_PI * (1 + dt / inhspan));
                    if (i < hspan + 2) {
                        printf("%d: dt %g inspan %g window %g sinc %g\n",
                               j, dt, inhspan, w, sinc);
                    }
                    sum += sinc * w * input[j];
                }
            }
            output[i] = sum / scale;
        }
        write_output("d1e.dat");
#endif
        // Reduce accumulated error from adding float phase:
        sine_fill(1000.0);
        Sample_ptr out = output;
        Sample_ptr inp = input;
        double offset = span;
        const int STEP = 100;  // convert 100 at a time
        for (int i = 0; i < OUTLEN; i += STEP) {
            resamp.resamp(out, STEP, inp, offset, 1.0, 1.0 / ratio);
            out += STEP;
            offset += STEP / ratio;
            int inp_step = std::floor(offset);
            inp += inp_step;
            offset -= inp_step;
        }
        write_output("d1e.dat");
        
        printf("Wrote d1a.dat, d1b.dat, d1c.dat, d1d.dat, d1e.dat\n");
    }
    
    if (strcmp(argv[3], "2") == 0) {
        // downsample from 88200Hz with sines at 20, 24, 28, 32, 36KHz
        const double ratio = 2.0;
        // low-pass ratio -- set filter cutoff at 18K instead of 22.05K
        const double lpratio = ratio * 22050 / 18000;
        
        sine_fill(10000.0);
        resamp.resamp(output, OUTLEN, input, span, lpratio, ratio);
        write_output("d2a.dat");

        sine_fill(12000.0);
        resamp.resamp(output, OUTLEN, input, span, lpratio, ratio);
        write_output("d2b.dat");

        sine_fill(14000.0);
        resamp.resamp(output, OUTLEN, input, span, lpratio, ratio);
        write_output("d2c.dat");

        sine_fill(16000.0);
        resamp.resamp(output, OUTLEN, input, span, lpratio, ratio);
        write_output("d2d.dat");
        
        sine_fill(18000.0);
        resamp.resamp(output, OUTLEN, input, span, lpratio, ratio);
        write_output("d2e.dat");

        printf("Wrote d2a.dat, d2b.dat, d2c.dat, d2d.dat, d2e.dat\n");
    }

    if (strcmp(argv[3], "3") == 0) {
        // frequency sweep
        const double ratio = 2.0;
        // low-pass ratio -- set filter cutoff at 18K instead of 22.05K
        const double lpratio = ratio * 22050 / 18000;

        sine_sweep(5000.0, 12000.0, 4.0);
        resamp.resamp(output, OUTLEN, input, span, lpratio, ratio);
        write_output("d3a.dat");
        printf("Wrote d3a.dat\n");
    }

    if (strcmp(argv[3], "4") == 0) {
        // execution time
        sine_fill(1000);
        printf("start\n");
        int reps = 20000;
        for (int i = 0; i < reps; i++) {
            resamp.resamp(output, OUTLEN, input, span, 1.0, 0.911);
        }
        printf("done with %d sec of sound at 44100kHz\n",
              (int) (reps * OUTLEN / AR));
    }
}
