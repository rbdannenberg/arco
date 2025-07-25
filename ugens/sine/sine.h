/* sine -- unit generator for arco
 *
 * generated by f2a.py
 */

/*------------- BEGIN FAUST PREAMBLE -------------*/

/* ------------------------------------------------------------
name: "sine"
Code generated with Faust 2.75.7 (https://faust.grame.fr)
Compilation options: -lang cpp -light -ct 1 -cn Sine -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 0
------------------------------------------------------------ */

#ifndef  __Sine_H__
#define  __Sine_H__

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <math.h>

#ifndef FAUSTCLASS 
#define FAUSTCLASS Sine
#endif

#ifdef __APPLE__ 
#define exp10f __exp10f
#define exp10 __exp10
#endif

#if defined(_WIN32)
#define RESTRICT __restrict
#else
#define RESTRICT __restrict__
#endif

class SineSIG0 {
    
  private:
    
    int iVec0[2];
    int iRec0[2];
    
  public:
    
    int getNumInputsSineSIG0() {
        return 0;
    }
    int getNumOutputsSineSIG0() {
        return 1;
    }
    
    void instanceInitSineSIG0(int sample_rate) {
        for (int l0 = 0; l0 < 2; l0 = l0 + 1) {
            iVec0[l0] = 0;
        }
        for (int l1 = 0; l1 < 2; l1 = l1 + 1) {
            iRec0[l1] = 0;
        }
    }
    
    void fillSineSIG0(int count, float* table) {
        for (int i1 = 0; i1 < count; i1 = i1 + 1) {
            iVec0[0] = 1;
            iRec0[0] = (iRec0[1] + iVec0[1]) % 65536;
            table[i1] = std::sin(9.58738e-05f * float(iRec0[0]));
            iVec0[1] = iVec0[0];
            iRec0[1] = iRec0[0];
        }
    }

};

static SineSIG0* newSineSIG0() { return (SineSIG0*)new SineSIG0(); }
static void deleteSineSIG0(SineSIG0* dsp) { delete dsp; }

static float ftbl0SineSIG0[65536];
/*-------------- END FAUST PREAMBLE --------------*/

extern const char *Sine_name;

class Sine : public Ugen {
public:
    struct Sine_state {
        int iVec1[2];
        float fRec1[2];
        Sample fSlow0_prev;
        Sample fSlow1_prev;
    };
    Vec<Sine_state> states;
    void (Sine::*run_channel)(Sine_state *state);

    Ugen_ptr freq;
    int freq_stride;
    Sample_ptr freq_samps;

    Ugen_ptr amp;
    int amp_stride;
    Sample_ptr amp_samps;

    float fConst0;

    Sine(int id, int nchans, Ugen_ptr freq_, Ugen_ptr amp_) :
            Ugen(id, 'a', nchans) {
        freq = freq_;
        amp = amp_;
        flags = CAN_TERMINATE;
        states.set_size(chans);
        fConst0 = 1.0f / std::min<float>(1.92e+05f, std::max<float>(1.0f, float(AR)));
        init_freq(freq);
        init_amp(amp);
        run_channel = (void (Sine::*)(Sine_state *)) 0;
        update_run_channel();
    }

    ~Sine() {
        freq->unref();
        amp->unref();
    }

    const char *classname() { return Sine_name; }

    void initialize_channel_states() {
        for (int i = 0; i < chans; i++) {
            for (int l2 = 0; l2 < 2; l2 = l2 + 1) {
                states[i].iVec1[l2] = 0;
            }
            for (int l3 = 0; l3 < 2; l3 = l3 + 1) {
                states[i].fRec1[l3] = 0.0f;
            }
            states[i].fSlow0_prev = 0.0f;
            states[i].fSlow1_prev = 0.0f;
        }
    }

    void update_run_channel() {
        // initialize run_channel based on input types
        void (Sine::*new_run_channel)(Sine_state *state);
        if (freq->rate == 'a') {
            if (amp->rate == 'a') {
                new_run_channel = &Sine::chan_aa_a;
            } else {
                new_run_channel = &Sine::chan_ab_a;
            }
        } else {
            if (amp->rate == 'a') {
                new_run_channel = &Sine::chan_ba_a;
            } else {
                new_run_channel = &Sine::chan_bb_a;
            }
        }
        if (new_run_channel != run_channel) {
            initialize_channel_states();
            run_channel = new_run_channel;
        }
    }

    void print_sources(int indent, bool print_flag) {
        freq->print_tree(indent, print_flag, "freq");
        amp->print_tree(indent, print_flag, "amp");
    }

    void repl_freq(Ugen_ptr ugen) {
        freq->unref();
        init_freq(ugen);
        update_run_channel();
    }

    void repl_amp(Ugen_ptr ugen) {
        amp->unref();
        init_amp(ugen);
        update_run_channel();
    }

    void set_freq(int chan, float f) {
        freq->const_set(chan, f, "Sine::set_freq");
    }

    void set_amp(int chan, float f) {
        amp->const_set(chan, f, "Sine::set_amp");
    }

    void init_freq(Ugen_ptr ugen) { init_param(ugen, freq, &freq_stride); }

    void init_amp(Ugen_ptr ugen) { init_param(ugen, amp, &amp_stride); }

    void chan_ba_a(Sine_state *state) {
        FAUSTFLOAT* input0 = amp_samps;
        FAUSTFLOAT* output0 = out_samps;
        float fSlow0 = fConst0 * float(freq_samps[0]);
        for (int i0 = 0; i0 < BL; i0 = i0 + 1) {
            state->iVec1[0] = 1;
            float fTemp0 = ((1 - state->iVec1[1]) ? 0.0f : fSlow0 + state->fRec1[1]);
            state->fRec1[0] = fTemp0 - std::floor(fTemp0);
            output0[i0] = FAUSTFLOAT(ftbl0SineSIG0[std::max<int>(0, std::min<int>(int(65536.0f * state->fRec1[0]), 65535))] * float(input0[i0]));
            state->iVec1[1] = state->iVec1[0];
            state->fRec1[1] = state->fRec1[0];
        }
    }

    void chan_bb_a(Sine_state *state) {
        FAUSTFLOAT* output0 = out_samps;
        float fSlow0 = float(amp_samps[0]);
        float fSlow1 = fConst0 * float(freq_samps[0]);
        Sample fSlow0_incr = (fSlow0 - state->fSlow0_prev) * BL_RECIP;
        Sample fSlow0_fast = state->fSlow0_prev;
        state->fSlow0_prev = fSlow0;
        Sample fSlow1_incr = (fSlow1 - state->fSlow1_prev) * BL_RECIP;
        Sample fSlow1_fast = state->fSlow1_prev;
        state->fSlow1_prev = fSlow1;
        for (int i0 = 0; i0 < BL; i0 = i0 + 1) {
            fSlow0_fast += fSlow0_incr;
            fSlow1_fast += fSlow1_incr;
            state->iVec1[0] = 1;
            float fTemp0 = ((1 - state->iVec1[1]) ? 0.0f : fSlow1_fast + state->fRec1[1]);
            state->fRec1[0] = fTemp0 - std::floor(fTemp0);
            output0[i0] = FAUSTFLOAT(fSlow0_fast * ftbl0SineSIG0[std::max<int>(0, std::min<int>(int(65536.0f * state->fRec1[0]), 65535))]);
            state->iVec1[1] = state->iVec1[0];
            state->fRec1[1] = state->fRec1[0];
        }
    }

    void chan_aa_a(Sine_state *state) {
        FAUSTFLOAT* input0 = freq_samps;
        FAUSTFLOAT* input1 = amp_samps;
        FAUSTFLOAT* output0 = out_samps;
        for (int i0 = 0; i0 < BL; i0 = i0 + 1) {
            state->iVec1[0] = 1;
            float fTemp0 = ((1 - state->iVec1[1]) ? 0.0f : state->fRec1[1] + fConst0 * float(input0[i0]));
            state->fRec1[0] = fTemp0 - std::floor(fTemp0);
            output0[i0] = FAUSTFLOAT(float(input1[i0]) * ftbl0SineSIG0[std::max<int>(0, std::min<int>(int(65536.0f * state->fRec1[0]), 65535))]);
            state->iVec1[1] = state->iVec1[0];
            state->fRec1[1] = state->fRec1[0];
        }
    }

    void chan_ab_a(Sine_state *state) {
        FAUSTFLOAT* input0 = freq_samps;
        FAUSTFLOAT* output0 = out_samps;
        float fSlow0 = float(amp_samps[0]);
        Sample fSlow0_incr = (fSlow0 - state->fSlow0_prev) * BL_RECIP;
        Sample fSlow0_fast = state->fSlow0_prev;
        state->fSlow0_prev = fSlow0;
        for (int i0 = 0; i0 < BL; i0 = i0 + 1) {
            fSlow0_fast += fSlow0_incr;
            state->iVec1[0] = 1;
            float fTemp0 = ((1 - state->iVec1[1]) ? 0.0f : state->fRec1[1] + fConst0 * float(input0[i0]));
            state->fRec1[0] = fTemp0 - std::floor(fTemp0);
            output0[i0] = FAUSTFLOAT(fSlow0_fast * ftbl0SineSIG0[std::max<int>(0, std::min<int>(int(65536.0f * state->fRec1[0]), 65535))]);
            state->iVec1[1] = state->iVec1[0];
            state->fRec1[1] = state->fRec1[0];
        }
    }

    void real_run() {
        freq_samps = freq->run(current_block);  // update input
        amp_samps = amp->run(current_block);  // update input
        Sine_state *state = &states[0];
        for (int i = 0; i < chans; i++) {
            (this->*run_channel)(state);
            state++;
            out_samps += BL;
            freq_samps += freq_stride;
            amp_samps += amp_stride;
        }
    }
};
#endif
