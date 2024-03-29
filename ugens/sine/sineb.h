/* sineb -- unit generator for arco
 *
 * generated by f2a.py
 */

/*------------- BEGIN FAUST PREAMBLE -------------*/

/* ------------------------------------------------------------
name: "sineb"
Code generated with Faust 2.59.6 (https://faust.grame.fr)
Compilation options: -lang cpp -os0 -fpga-mem 10000 -light -ct 1 -cn Sineb -es 1 -mcd 16 -single -ftz 0
------------------------------------------------------------ */

#ifndef  __Sineb_H__
#define  __Sineb_H__

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <math.h>

static float ftbl0SinebSIG0[65536];

#ifndef FAUSTCLASS 
#define FAUSTCLASS Sineb
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

#define FAUST_INT_CONTROLS 0
#define FAUST_REAL_CONTROLS 2
/*-------------- END FAUST PREAMBLE --------------*/

extern const char *Sineb_name;

class Sineb : public Ugen {
public:
    struct Sineb_state {
        FAUSTFLOAT fEntry0;
        int iVec1[2];
        FAUSTFLOAT fEntry1;
        float fRec1[2];
        int iVec0[2];
        int iRec0[2];
    };
    Vec<Sineb_state> states;
    void (Sineb::*run_channel)(Sineb_state *state);

    Ugen_ptr freq;
    int freq_stride;
    Sample_ptr freq_samps;

    Ugen_ptr amp;
    int amp_stride;
    Sample_ptr amp_samps;

    float fConst0;

    Sineb(int id, int nchans, Ugen_ptr freq_, Ugen_ptr amp_) :
            Ugen(id, 'b', nchans) {
        freq = freq_;
        amp = amp_;
        flags = CAN_TERMINATE;
        states.set_size(chans);
        fConst0 = 1.0f / std::min<float>(1.92e+05f, std::max<float>(1.0f, float(BR)));
        init_freq(freq);
        init_amp(amp);
    }

    ~Sineb() {
        freq->unref();
        amp->unref();
    }

    const char *classname() { return Sineb_name; }

    void initialize_channel_states() {
        for (int i = 0; i < chans; i++) {
            for (int l2 = 0; l2 < 2; l2 = l2 + 1) {
                states[i].iVec1[l2] = 0;
            }
            for (int l3 = 0; l3 < 2; l3 = l3 + 1) {
                states[i].fRec1[l3] = 0.0f;
            }
        }
    }

    void print_sources(int indent, bool print_flag) {
        freq->print_tree(indent, print_flag, "freq");
        amp->print_tree(indent, print_flag, "amp");
    }

    void repl_freq(Ugen_ptr ugen) {
        freq->unref();
        init_freq(ugen);
    }

    void repl_amp(Ugen_ptr ugen) {
        amp->unref();
        init_amp(ugen);
    }

    void set_freq(int chan, float f) {
        freq->const_set(chan, f, "Sineb::set_freq");
    }

    void set_amp(int chan, float f) {
        amp->const_set(chan, f, "Sineb::set_amp");
    }

    void init_freq(Ugen_ptr ugen) { init_param(ugen, freq, freq_stride); }

    void init_amp(Ugen_ptr ugen) { init_param(ugen, amp, amp_stride); }

    void real_run() {
        freq_samps = freq->run(current_block); // update input
        amp_samps = amp->run(current_block); // update input
        Sineb_state *state = &states[0];
        for (int i = 0; i < chans; i++) {
            FAUSTFLOAT tmp_0 = float(*amp_samps);
            FAUSTFLOAT tmp_1 = fConst0 * float(*freq_samps);
            state->iVec1[0] = 1;
            float fTemp0 = ((1 - state->iVec1[1]) ? 0.0f : tmp_1 + state->fRec1[1]);
            state->fRec1[0] = fTemp0 - std::floor(fTemp0);
            *out_samps++ = FAUSTFLOAT(tmp_0 * ftbl0SinebSIG0[std::max<int>(0, std::min<int>(int(65536.0f * state->fRec1[0]), 65535))]);
            state->iVec1[1] = state->iVec1[0];
            state->fRec1[1] = state->fRec1[0];            state++;
            freq_samps += freq_stride;
            amp_samps += amp_stride;
        }
    }
};
#endif
