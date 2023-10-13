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
    int intap;    // where to insert sample
    int fouttap;  // where to take interpolated output
    float fouttap_delta;

    struct Ola_pitch_shift_state {
       Ringbuf delaybuf;
    };
    Vec<Ola_pitch_shift_state> states;

    Ugen_ptr input;
    int input_stride;
    Sample_ptr input_samps;


    Ola_pitch_shift(int id, int nchans, Ugen_ptr input, float ratio,
                    float xfade, float windur) : Ugen(id, 'a', nchans) {
        ixfade = 1;
        iwindur = 1;
        xfade_recip = 1;
        intap = 0;
        fouttap = 0;
        fouttap_delta = 0;
        states.set_size(chans);
        for (int i = 0; i < chans; i++) {
            states[i].delaybuf.init(0);
        }
        set_ratio(ratio);
        set_windur(windur);
        set_xfade(xfade);

        intap = 0;
        fouttap = 0.0;
        init_input(input);
    }

    ~Ola_pitch_shift() {
        input->unref();
        for (int i = 0; i < chans; i++) {
            states[i].delaybuf.finish();
        }
    }

    const char *classname() { return Ola_pitch_shift_name; }

    
    void print_details(int indent) {
        arco_print("ratio %g xfade %g windur %g", ratio, xfade, windur);
    }


    void init_input(Ugen_ptr ugen) { init_param(ugen, input, input_stride); }

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
        input->unref();
        init_input(ugen);
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
            state->delaybuf.set_fifo_len(bl, true);
        }
        fouttap = 0.0;
    }

    void set_windur(float w) {
        windur = w;
        set_buflen();
    }

    void chan_a(Ola_pitch_shift_state *state) {
        Ringbuf &delaybuf = state->delaybuf;

        for (int i = 0; i < BL; i++)  {
            delaybuf.toss(1);  // make room for mor
            delaybuf.enqueue(input_samps[i]);

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
            int tap1a = (int) fouttap_delta;
            float alpha = fouttap_delta - tap1a;  // fractional part
            int tap2a = tap1a + 1;
            // fouttap is an offset from input (tail) and it is negative.
            // get_nth(n) goes back to nth previous sample, n is positive.
            // so we have to flip the sign by using -tap1a and -tap2a
            float x1a = delaybuf.get_nth(-tap1a);
            float x2a = delaybuf.get_nth(-tap2a);
            float xa = x1a +  alpha * (x2a - x1a);
         
            if (fouttap_delta < -ixfade) { 
                *out_samps++ = xa;  // and we're done
            } else {
                int tap1b = tap1a - (iwindur - ixfade);
                int tap2b = tap1b + 1;
                float x1b = delaybuf.get_nth(-tap1b);
                float x2b = delaybuf.get_nth(-tap2b);
                // note that since we are at an integer offset, the fractional
                // part of tap1b is alpha, the fractional part of tap1a
                float xb = x1b + alpha * (x2b - x1b);
                // this different interpolation is to compute xfade:
                float beta = -fouttap_delta * xfade_recip;
                *out_samps++ = xb + beta * (xa - xb);
            }
            fouttap_delta += (ratio - 1.0F);
        }
    }
    
    void real_run() {
        input_samps = input->run(current_block);
        for (int i = 0; i < chans; i++) {
            chan_a(&states[i]);
            input_samps += input_stride;
        }
    }

};

#endif

