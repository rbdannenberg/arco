/* sine -- unit generator for arco
 *
 * generated by f2a.py
 */

/*------------- BEGIN FAUST PREAMBLE -------------*/

/* ------------------------------------------------------------
name: "sine"
Code generated with Faust 2.37.3 (https://faust.grame.fr)
Compilation options: -lang cpp -light -es 1 -single -ftz 0
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
        for (int l0 = 0; (l0 < 2); l0 = (l0 + 1)) {
            iVec0[l0] = 0;
        }
        for (int l1 = 0; (l1 < 2); l1 = (l1 + 1)) {
            iRec0[l1] = 0;
        }
    }
    
    void fillSineSIG0(int count, float* table) {
        for (int i1 = 0; (i1 < count); i1 = (i1 + 1)) {
            iVec0[0] = 1;
            iRec0[0] = ((iVec0[1] + iRec0[1]) % 65536);
            table[i1] = std::sin((9.58738019e-05f * float(iRec0[0])));
            iVec0[1] = iVec0[0];
            iRec0[1] = iRec0[0];
        }
    }

};

static SineSIG0* newSineSIG0() { return (SineSIG0*)new SineSIG0(); }
static void deleteSineSIG0(SineSIG0* dsp) { delete dsp; }

static float ftbl0SineSIG0[65536];

#ifndef FAUSTCLASS 
#define FAUSTCLASS Sine
#endif

#ifdef __APPLE__ 
#define exp10f __exp10f
#define exp10 __exp10
#endif
/*-------------- END FAUST PREAMBLE --------------*/

extern const char *Sine_name;

class Sine : public Ugen {
public:
    struct Sine_state {
        float fRec1[2];
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

    Sine(int id, int nchans, Ugen_ptr freq_, Ugen_ptr amp_) : Ugen(id, 'a', nchans) {
        freq = freq_;
        amp = amp_;
        states.init(chans);
        fConst0 = (1.0f / std::min<float>(192000.0f, std::max<float>(1.0f, float(AR))));

        // initialize channel states
        for (int i = 0; i < chans; i++) {
            for (int l2 = 0; (l2 < 2); l2 = (l2 + 1)) {
                states[i].fRec1[l2] = 0.0f;
            }
        }

        init_freq(freq);
        init_amp(amp);
        update_run_channel();
    }

    ~Sine() {
        freq->unref();
        amp->unref();
    }

    const char *classname() { return Sine_name; }

    void update_run_channel() {
        // initialize run_channel based on input types
        if (freq->rate != 'a' && amp->rate == 'a') {
            run_channel = &Sine::chan_ba_a;
        } else if (freq->rate != 'a' && amp->rate != 'a') {
            run_channel = &Sine::chan_bb_a;
        } else if (freq->rate == 'a' && amp->rate == 'a') {
            run_channel = &Sine::chan_aa_a;
        } else if (freq->rate == 'a' && amp->rate != 'a') {
            run_channel = &Sine::chan_ab_a;
        }
        else {
            if (freq->rate != 'a') {
                freq = new Upsample(-1, freq->chans, freq);
            }
            if (amp->rate != 'a') {
                amp = new Upsample(-1, amp->chans, amp);
            }
            run_channel = &Sine::chan_aa_a;
        }
    }

    void print_sources(int indent, bool print) {
        freq->print_tree(indent, print, "freq");
        amp->print_tree(indent, print, "amp");
    }

    void repl_freq(Ugen_ptr inp) {
        freq->unref();
        init_freq(inp);
        update_run_channel();
    }

    void repl_amp(Ugen_ptr inp) {
        amp->unref();
        init_amp(inp);
        update_run_channel();
    }

    void set_freq(int chan, float f) {
        assert(freq->rate == 'c');
        freq->output[chan] = f;
    }

    void set_amp(int chan, float f) {
        assert(amp->rate == 'c');
        amp->output[chan] = f;
    }

    void init_freq(Ugen_ptr ugen) { init_param(ugen, freq, freq_stride); }

    void init_amp(Ugen_ptr ugen) { init_param(ugen, amp, amp_stride); }

    void chan_ba_a(Sine_state *state) {
        FAUSTFLOAT* input0 = amp_samps;
        float fSlow0 = (fConst0 * float(*freq_samps));
        for (int i0 = 0; (i0 < BL); i0 = (i0 + 1)) {
            state->fRec1[0] = (fSlow0 + (state->fRec1[1] - std::floor((fSlow0 + state->fRec1[1]))));
            *out_samps++ = FAUSTFLOAT((float(input0[i0]) * ftbl0SineSIG0[int((65536.0f * state->fRec1[0]))]));
            state->fRec1[1] = state->fRec1[0];
        }
    }

    void chan_bb_a(Sine_state *state) {
        float fSlow0 = float(*amp_samps);
        float fSlow1 = (fConst0 * float(*freq_samps));
        for (int i0 = 0; (i0 < BL); i0 = (i0 + 1)) {
            state->fRec1[0] = (fSlow1 + (state->fRec1[1] - std::floor((fSlow1 + state->fRec1[1]))));
            *out_samps++ = FAUSTFLOAT((fSlow0 * ftbl0SineSIG0[int((65536.0f * state->fRec1[0]))]));
            state->fRec1[1] = state->fRec1[0];
        }
    }

    void chan_aa_a(Sine_state *state) {
        FAUSTFLOAT* input0 = freq_samps;
        FAUSTFLOAT* input1 = amp_samps;
        for (int i0 = 0; (i0 < BL); i0 = (i0 + 1)) {
            float fTemp0 = (state->fRec1[1] + (fConst0 * float(input0[i0])));
            state->fRec1[0] = (fTemp0 - std::floor(fTemp0));
            *out_samps++ = FAUSTFLOAT((float(input1[i0]) * ftbl0SineSIG0[int((65536.0f * state->fRec1[0]))]));
            state->fRec1[1] = state->fRec1[0];
        }
    }

    void chan_ab_a(Sine_state *state) {
        FAUSTFLOAT* input0 = freq_samps;
        float fSlow0 = float(*amp_samps);
        for (int i0 = 0; (i0 < BL); i0 = (i0 + 1)) {
            float fTemp0 = (state->fRec1[1] + (fConst0 * float(input0[i0])));
            state->fRec1[0] = (fTemp0 - std::floor(fTemp0));
            *out_samps++ = FAUSTFLOAT((fSlow0 * ftbl0SineSIG0[int((65536.0f * state->fRec1[0]))]));
            state->fRec1[1] = state->fRec1[0];
        }
    }

    void real_run() {
        freq_samps = freq->run(current_block); // update input
        amp_samps = amp->run(current_block); // update input
        Sine_state *state = &states[0];
        for (int i = 0; i < chans; i++) {
            (this->*run_channel)(state);
            state++;
            freq_samps += freq_stride;
            amp_samps += amp_stride;
        }
    }
};
#endif