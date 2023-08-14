// olapitchshift.h -- overlap-add pitch shift
//
// Roger B. Dannenberg
// June 2023
//
// based on previous implementation pshift.ug from Aura
// There is only one variant that takes audio rate input
// Parameters are all setable with messages.
//
// Algorithm:
//
// Think of the buffer as a circle with a cross-fade when you
// pass the input pointer. Unrolled, the circle looks like:
//
//  x   x   x             input             x   x   x   x
//              x           |           x
//                   x      |       x
//                      x   V   x
//  |---|---|---|---|---|---x---|---|---|---|---|---|---|
//          |               |
//          |<------------->|
//               ixfade
//
//  If we unroll this differently, we get this:
//
//              x   x   x   x   x   x   x             input
//          x                               x           |
//      x                                        x      |
//  x                                               x   V
//  |---|---|---|---|---|---|---|---|---|---|---|---|---x
//                                      |               |
//  |<--------------------------------->|<------------->|
//       iwindur - ixfade                   ixfade
//  |<------------------------------------------------->|
//              iwindur
//
// Assume input is inserted first. When the output pointer 
// is more than ixfade from input, we just interpolate and
// return the value. When output pointer (a float) is within
// ixfade of input, we do a lookup at (fouttap - iwindur) + ixfade.
// and output a weighted sum based on one tap ramping up and
// the other tap ramping down. When the output pointer passes
// the input, then we drop back to the tap that just finished
// ramping up.
//     To keep track of things, the output tap is represented
// as an offset from input: fouttap_delta
// Note that the buffer len must be at least iwindur + 1, and
// iwindur must be at least 2 * ixfade
//     Note that grain duration depends on ratio -- when we
// shift pitch down, grains get longer. Shifting up, grains
// get shorter.
//    When there is pitch shifting, fouttap_delta will increase
// if ratio > 1 (shift up) and decrease if ratio < 1 (shift down).
// fouttap_delta is updated by ratio - 1.  Thus, if the pitch shift
// ratio = 1, we read at a constant offset from intap.  When there
// is pitch shifting, fouttap_delta will increase if ratio > 1
// (shift up) and decrease if ratio < 1 (shift down).
//     Of course, fouttap_delta must resolve to a position within
// the buffer, so the "real" time(s) where we pull samples from
// the buffer must be between the time of intap (where new samples
// are inserted, so the time is "now") and this time - windur,
// the oldest sample time in the buffer.
//     Notice that the time between grains is iwindur - ixfade,
// because of the overlap of ixfade. Thus, we want to keep
// fouttap_delta between -(iwindur-ixfade) and 0. This is the
// offset from intap, so to find the actual buffer location,
// we add intap and then compute the value modulo buffer length
// by masking with bufmask. (Buffer length is always a power of 2.)

/*
 * in:  in ratio xfade window
 * out: out
*/

#ifndef __OLAPITCHSHIFT_H__
#define __OLAPITCHSHIFT_H__

extern const char *Ola_pitch_shift_name;

class Ola_pitch_shift: public Ugen {
  public:
    float ratio;
    float xfade;
    float windur;
    int ixfade;   // xfade measured in samples
    float xfade_recip;  // used in inner loop
    int iwindur;  // windur measured in samples
    int bufmask;  // used to wrap around circular buffer
    int intap;    // where to insert sample
    int fouttap;  // where to take interpolated output
    float fouttap_delta;

    struct Ola_pitch_shift_state {
        Vec<float> delaybuf;
    };
    Vec<Ola_pitch_shift_state> states;

    Ugen_ptr inp;
    int inp_stride;
    Sample_ptr inp_samps;


    Ola_pitch_shift(int id, int nchans, Ugen_ptr inp_, float ratio,
                    float xfade, float windur) : Ugen(id, 'a', nchans) {
        inp = inp_;
        ixfade = 1;
        iwindur = 1;
        xfade_recip = 1;
        intap = 0;
        fouttap = 0;
        fouttap_delta = 0;
        states.init(chans);
        for (int i = 0; i < chans; i++) {
            states[i].delaybuf.init(0);
        }
        set_ratio(ratio);
        set_windur(windur);
        set_xfade(xfade);

        intap = 0;
        fouttap = 0.0;
        init_inp(inp);
    }

    ~Ola_pitch_shift() {
        inp->unref();
        for (int i = 0; i < chans; i++) {
            states[i].delaybuf.finish();
        }
    }

    const char *classname() { return Ola_pitch_shift_name; }
    
    void init_inp(Ugen_ptr ugen) { init_param(ugen, inp, inp_stride); }

    void set_ratio(float r) {
        ratio = r;
    }

    void set_xfade(float xf) {
        xfade = xf;
        ixfade = (int) (xfade * AR);
        if (ixfade < 1) {
            ixfade = 1;
        }
        xfade_recip = 1.0 / ixfade;
        if (iwindur < ixfade * 2) {
            set_buflen();
        }
    }
        
    
    void repl_inp(Ugen_ptr ugen) {
        inp->unref();
        init_inp(ugen);
    }

    
    void set_buflen() {
        if (windur == 0.0) return;
        iwindur = (int) (windur * AR);
        if (iwindur < ixfade * 2) {
            iwindur = ixfade * 2;
        }
        int bl = iwindur + 1;  // bl = delay buffer len in samples
        for (int i = 0; i < chans; i++) {
            Ola_pitch_shift_state *state = &states[i];
            if (state->delaybuf.size() < bl) {  // expand delaybuf
                int len = 32;
                while (len < bl) len <<= 1;
                bufmask = len - 1;
                state->delaybuf.finish(); // reallocate
                printf("ola_pitch_shift reallocate delaybuf chan %d length %d"
                       " bufmask %x\n", i, len, bufmask);
                state->delaybuf.init(len, true);  // fill new buffer with zeros
            } else {
                printf("ola_pitch_shift reuse delaybuf chan %d (size %d)"
                       " bufmask %x\n", i, state->delaybuf.size(), bufmask);
                state->delaybuf.zero();
            }
        }
        intap = 0;
        fouttap = 0.0;
    }

    void set_windur(float w) {
        windur = w;
        set_buflen();
    }

    void chan_a(Ola_pitch_shift_state *state) {
        Vec<Sample> &delaybuf = state->delaybuf;

        for (int i = 0; i < BL; i++)  {
            assert(delaybuf.bounds_check(intap));
            delaybuf[intap++] = inp_samps[i];
            intap &= bufmask;

            // adjust fouttap_delta to the range -(iwindur-ixfade) to 0.
            // when it exceeds either extreme, we wrap around, and due
            // to overlap the jump amount is iwindur-ixfade.
            if (fouttap_delta > 0) {
                fouttap_delta -= (iwindur - ixfade);
            } else if (fouttap_delta < -(iwindur - ixfade)) {
                fouttap_delta += (iwindur - ixfade);
            }
            // see diagram above. Need to crossfade when fouttap_delta
            // is in the range -ixfade to 0. But we always need at least
            // one sample, which we get first:
            float fouttap = intap + delaybuf.size() + fouttap_delta;
            // delaybuf->size() was added to ensure that fouttap is positive
            int tap1a = (int) fouttap;
            float alpha = fouttap - tap1a;  // fractional part for interpolation
            tap1a &= bufmask;
            int tap2a = (tap1a + 1) & bufmask;
            assert(delaybuf.bounds_check(tap1a));
            assert(delaybuf.bounds_check(tap2a));
            float x1a = delaybuf[tap1a];
            float x2a = delaybuf[tap2a];
            float xa = x1a +  alpha * (x2a - x1a);
         
            if (fouttap_delta < -ixfade) { 
                *out_samps++ = xa;  // and we're done
            } else {
                int tap1b = (tap1a + delaybuf.size() - (iwindur - ixfade)) &
                            bufmask;
                int tap2b = (tap1b + 1) & bufmask;
             
                assert(delaybuf.bounds_check(tap1b));
                assert(delaybuf.bounds_check(tap2b));
                float x1b = delaybuf[tap1b];
                float x2b = delaybuf[tap2b];
                // note that since we are at an integer offset, the fractional
                // part of tap1b is alpha, the fractional part of tap1a
                float xb = x1b + alpha * (x2b - x1b);
                // another interpolation, this time interpolating to compute xfade
                float beta = -fouttap_delta * xfade_recip;
                *out_samps++ = xb + beta * (xa - xb);
            }
            fouttap_delta += (ratio - 1.0F);
        }
    }
    
    void real_run() {
        inp_samps = inp->run(current_block);
        for (int i = 0; i < chans; i++) {
            chan_a(&states[i]);
            inp_samps += inp_stride;
        }
    }

};

#endif

