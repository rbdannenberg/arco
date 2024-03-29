/* reson -- unit generator for arco
 *
 * generated by f2a.py
 */

/*------------- BEGIN FAUST PREAMBLE -------------*/

/* ------------------------------------------------------------
name: "reson"
Code generated with Faust 2.59.6 (https://faust.grame.fr)
Compilation options: -lang cpp -light -ct 1 -cn Reson -es 1 -mcd 16 -single -ftz 0
------------------------------------------------------------ */

#ifndef  __Reson_H__
#define  __Reson_H__

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <math.h>

#ifndef FAUSTCLASS 
#define FAUSTCLASS Reson
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

static float Reson_faustpower2_f(float value) {
    return value * value;
}
/*-------------- END FAUST PREAMBLE --------------*/

extern const char *Reson_name;

class Reson : public Ugen {
public:
    struct Reson_state {
        float fRec0[3];
    };
    Vec<Reson_state> states;
    void (Reson::*run_channel)(Reson_state *state);

    Ugen_ptr snd;
    int snd_stride;
    Sample_ptr snd_samps;

    Ugen_ptr center;
    int center_stride;
    Sample_ptr center_samps;

    Ugen_ptr q;
    int q_stride;
    Sample_ptr q_samps;

    float fConst0;

    Reson(int id, int nchans, Ugen_ptr snd_, Ugen_ptr center_, Ugen_ptr q_) :
            Ugen(id, 'a', nchans) {
        snd = snd_;
        center = center_;
        q = q_;
        flags = CAN_TERMINATE;
        states.set_size(chans);
        fConst0 = 3.1415927f / std::min<float>(1.92e+05f, std::max<float>(1.0f, float(AR)));
        init_snd(snd);
        init_center(center);
        init_q(q);
        run_channel = (void (Reson::*)(Reson_state *)) 0;
        update_run_channel();
    }

    ~Reson() {
        snd->unref();
        center->unref();
        q->unref();
    }

    const char *classname() { return Reson_name; }

    void initialize_channel_states() {
        for (int i = 0; i < chans; i++) {
            for (int l0 = 0; l0 < 3; l0 = l0 + 1) {
                states[i].fRec0[l0] = 0.0f;
            }
        }
    }

    void update_run_channel() {
        // initialize run_channel based on input types
        void (Reson::*new_run_channel)(Reson_state *state);
        if (snd->rate == 'a' && center->rate != 'a' && q->rate != 'a') {
            new_run_channel = &Reson::chan_abb_a;
        } else if (snd->rate == 'a' && center->rate != 'a' && q->rate == 'a') {
            new_run_channel = &Reson::chan_aba_a;
        } else if (snd->rate == 'a' && center->rate == 'a' && q->rate != 'a') {
            new_run_channel = &Reson::chan_aab_a;
        } else if (snd->rate == 'a' && center->rate == 'a' && q->rate == 'a') {
            new_run_channel = &Reson::chan_aaa_a;
        } else {
            if (snd->rate != 'a') {
                snd = new Upsample(-1, snd->chans, snd);
            }
            if (center->rate != 'a') {
                center = new Upsample(-1, center->chans, center);
            }
            if (q->rate != 'a') {
                q = new Upsample(-1, q->chans, q);
            }
            new_run_channel = &Reson::chan_aaa_a;
        }
        if (new_run_channel != run_channel) {
            initialize_channel_states();
            run_channel = new_run_channel;
        }
    }

    void print_sources(int indent, bool print_flag) {
        snd->print_tree(indent, print_flag, "snd");
        center->print_tree(indent, print_flag, "center");
        q->print_tree(indent, print_flag, "q");
    }

    void repl_snd(Ugen_ptr ugen) {
        snd->unref();
        init_snd(ugen);
        update_run_channel();
    }

    void repl_center(Ugen_ptr ugen) {
        center->unref();
        init_center(ugen);
        update_run_channel();
    }

    void repl_q(Ugen_ptr ugen) {
        q->unref();
        init_q(ugen);
        update_run_channel();
    }

    void set_snd(int chan, float f) {
        snd->const_set(chan, f, "Reson::set_snd");
    }

    void set_center(int chan, float f) {
        center->const_set(chan, f, "Reson::set_center");
    }

    void set_q(int chan, float f) {
        q->const_set(chan, f, "Reson::set_q");
    }

    void init_snd(Ugen_ptr ugen) { init_param(ugen, snd, snd_stride); }

    void init_center(Ugen_ptr ugen) { init_param(ugen, center, center_stride); }

    void init_q(Ugen_ptr ugen) { init_param(ugen, q, q_stride); }

    void chan_abb_a(Reson_state *state) {
        FAUSTFLOAT* input0 = snd_samps;
        float fSlow0 = 1.0f / std::max<float>(float(*q_samps), 0.1f);
        float fSlow1 = std::tan(fConst0 * std::max<float>(float(*center_samps), 0.1f));
        float fSlow2 = 1.0f / fSlow1;
        float fSlow3 = 1.0f / ((fSlow0 + fSlow2) / fSlow1 + 1.0f);
        float fSlow4 = (fSlow2 - fSlow0) / fSlow1 + 1.0f;
        float fSlow5 = 2.0f * (1.0f - 1.0f / Reson_faustpower2_f(fSlow1));
        for (int i0 = 0; i0 < BL; i0 = i0 + 1) {
            state->fRec0[0] = float(input0[i0]) - fSlow3 * (fSlow4 * state->fRec0[2] + fSlow5 * state->fRec0[1]);
            *out_samps++ = FAUSTFLOAT(fSlow3 * (state->fRec0[2] + state->fRec0[0] + 2.0f * state->fRec0[1]));
            state->fRec0[2] = state->fRec0[1];
            state->fRec0[1] = state->fRec0[0];
        }
    }

    void chan_aba_a(Reson_state *state) {
        FAUSTFLOAT* input0 = snd_samps;
        FAUSTFLOAT* input1 = q_samps;
        float fSlow0 = std::tan(fConst0 * std::max<float>(float(*center_samps), 0.1f));
        float fSlow1 = 1.0f / fSlow0;
        float fSlow2 = 2.0f * (1.0f - 1.0f / Reson_faustpower2_f(fSlow0));
        for (int i0 = 0; i0 < BL; i0 = i0 + 1) {
            float fTemp0 = 1.0f / std::max<float>(float(input1[i0]), 0.1f);
            float fTemp1 = fSlow1 * (fSlow1 + fTemp0) + 1.0f;
            state->fRec0[0] = float(input0[i0]) - (state->fRec0[2] * (fSlow1 * (fSlow1 - fTemp0) + 1.0f) + fSlow2 * state->fRec0[1]) / fTemp1;
            *out_samps++ = FAUSTFLOAT((state->fRec0[2] + state->fRec0[0] + 2.0f * state->fRec0[1]) / fTemp1);
            state->fRec0[2] = state->fRec0[1];
            state->fRec0[1] = state->fRec0[0];
        }
    }

    void chan_aab_a(Reson_state *state) {
        FAUSTFLOAT* input0 = snd_samps;
        FAUSTFLOAT* input1 = center_samps;
        float fSlow0 = 1.0f / std::max<float>(float(*q_samps), 0.1f);
        for (int i0 = 0; i0 < BL; i0 = i0 + 1) {
            float fTemp0 = std::tan(fConst0 * std::max<float>(float(input1[i0]), 0.1f));
            float fTemp1 = 1.0f / fTemp0;
            float fTemp2 = (fSlow0 + fTemp1) / fTemp0 + 1.0f;
            state->fRec0[0] = float(input0[i0]) - (state->fRec0[2] * ((fTemp1 - fSlow0) / fTemp0 + 1.0f) + 2.0f * state->fRec0[1] * (1.0f - 1.0f / Reson_faustpower2_f(fTemp0))) / fTemp2;
            *out_samps++ = FAUSTFLOAT((state->fRec0[2] + state->fRec0[0] + 2.0f * state->fRec0[1]) / fTemp2);
            state->fRec0[2] = state->fRec0[1];
            state->fRec0[1] = state->fRec0[0];
        }
    }

    void chan_aaa_a(Reson_state *state) {
        FAUSTFLOAT* input0 = snd_samps;
        FAUSTFLOAT* input1 = center_samps;
        FAUSTFLOAT* input2 = q_samps;
        for (int i0 = 0; i0 < BL; i0 = i0 + 1) {
            float fTemp0 = std::tan(fConst0 * std::max<float>(float(input1[i0]), 0.1f));
            float fTemp1 = 1.0f / fTemp0;
            float fTemp2 = 1.0f / std::max<float>(float(input2[i0]), 0.1f);
            float fTemp3 = (fTemp2 + fTemp1) / fTemp0 + 1.0f;
            state->fRec0[0] = float(input0[i0]) - (state->fRec0[2] * ((fTemp1 - fTemp2) / fTemp0 + 1.0f) + 2.0f * state->fRec0[1] * (1.0f - 1.0f / Reson_faustpower2_f(fTemp0))) / fTemp3;
            *out_samps++ = FAUSTFLOAT((state->fRec0[2] + state->fRec0[0] + 2.0f * state->fRec0[1]) / fTemp3);
            state->fRec0[2] = state->fRec0[1];
            state->fRec0[1] = state->fRec0[0];
        }
    }

    void real_run() {
        snd_samps = snd->run(current_block); // update input
        center_samps = center->run(current_block); // update input
        q_samps = q->run(current_block); // update input
        if (((snd->flags) & TERMINATED) &&
            (flags & CAN_TERMINATE)) {
            terminate();
        }
        Reson_state *state = &states[0];
        for (int i = 0; i < chans; i++) {
            (this->*run_channel)(state);
            state++;
            snd_samps += snd_stride;
            center_samps += center_stride;
            q_samps += q_stride;
        }
    }
};
#endif
