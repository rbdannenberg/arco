/* mult -- unit generator for arco
 *
 * generated by f2a.py
 */

/*------------- BEGIN FAUST PREAMBLE -------------*/

/* ------------------------------------------------------------
name: "mult"
Code generated with Faust 2.37.3 (https://faust.grame.fr)
Compilation options: -lang cpp -light -es 1 -single -ftz 0
------------------------------------------------------------ */

#ifndef  __Mult_H__
#define  __Mult_H__

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

#include <algorithm>
#include <cmath>
#include <cstdint>


#ifndef FAUSTCLASS 
#define FAUSTCLASS Mult
#endif

#ifdef __APPLE__ 
#define exp10f __exp10f
#define exp10 __exp10
#endif
/*-------------- END FAUST PREAMBLE --------------*/

extern const char *Mult_name;

class Mult : public Ugen {
public:
    struct Mult_state {
        float x1_prev;
        float x2_prev;
    };
    Vec<Mult_state> states;
    void (Mult::*run_channel)(Mult_state *state);

    Ugen_ptr x1;
    int x1_stride;
    Sample_ptr x1_samps;

    Ugen_ptr x2;
    int x2_stride;
    Sample_ptr x2_samps;


    Mult(int id, int nchans, Ugen_ptr x1_, Ugen_ptr x2_) : Ugen(id, 'a', nchans) {
        x1 = x1_;
        x2 = x2_;
        states.init(chans);


        // initialize channel states
        for (int i = 0; i < chans; i++) {

            states[i].x1_prev = 0.0f;
            states[i].x2_prev = 0.0f;
        }

        init_x1(x1);
        init_x2(x2);
        update_run_channel();
    }

    ~Mult() {
        x1->unref();
        x2->unref();
    }

    const char *classname() { return Mult_name; }

    void update_run_channel() {
        // initialize run_channel based on input types
        if (x1->rate == 'a' && x2->rate == 'a') {
            run_channel = &Mult::chan_aa_a;
        } else if (x1->rate == 'a' && x2->rate != 'a') {
            run_channel = &Mult::chan_ab_a;
        } else if (x1->rate != 'a' && x2->rate == 'a') {
            run_channel = &Mult::chan_ba_a;
        } else if (x1->rate != 'a' && x2->rate != 'a') {
            run_channel = &Mult::chan_bb_a;
        }
        else {
            if (x1->rate != 'a') {
                x1 = new Upsample(-1, x1->chans, x1);
            }
            if (x2->rate != 'a') {
                x2 = new Upsample(-1, x2->chans, x2);
            }
            run_channel = &Mult::chan_aa_a;
        }
    }

    void print_sources(int indent, bool print) {
        x1->print_tree(indent, print, "x1");
        x2->print_tree(indent, print, "x2");
    }

    void repl_x1(Ugen_ptr inp) {
        x1->unref();
        init_x1(inp);
        update_run_channel();
    }

    void repl_x2(Ugen_ptr inp) {
        x2->unref();
        init_x2(inp);
        update_run_channel();
    }

    void set_x1(int chan, float f) {
        assert(x1->rate == 'c');
        x1->output[chan] = f;
    }

    void set_x2(int chan, float f) {
        assert(x2->rate == 'c');
        x2->output[chan] = f;
    }

    void init_x1(Ugen_ptr ugen) { init_param(ugen, x1, x1_stride); }

    void init_x2(Ugen_ptr ugen) { init_param(ugen, x2, x2_stride); }

    void chan_aa_a(Mult_state *state) {
        FAUSTFLOAT* input0 = x1_samps;
        FAUSTFLOAT* input1 = x2_samps;
        for (int i0 = 0; (i0 < BL); i0 = (i0 + 1)) {
            *out_samps++ = FAUSTFLOAT((float(input0[i0]) * float(input1[i0])));
        }
    }

    void chan_ab_a(Mult_state *state) {
        FAUSTFLOAT* input0 = x1_samps;
        float fSlow0 = float(*x2_samps);
        Sample x2_incr = (fSlow0 - state->x2_prev) * BL_RECIP;
        Sample x2_prev = state->x2_prev;
        state->x2_prev = fSlow0;
        for (int i0 = 0; (i0 < BL); i0 = (i0 + 1)) {
            x2_prev += x2_incr;
            *out_samps++ = FAUSTFLOAT((x2_prev * float(input0[i0])));
        }
    }

    void chan_ba_a(Mult_state *state) {
        FAUSTFLOAT* input0 = x2_samps;
        float fSlow0 = float(*x1_samps);
        Sample x1_incr = (fSlow0 - state->x1_prev) * BL_RECIP;
        Sample x1_prev = state->x1_prev;
        state->x1_prev = fSlow0;
        for (int i0 = 0; (i0 < BL); i0 = (i0 + 1)) {
            x1_prev += x1_incr;
            *out_samps++ = FAUSTFLOAT((x1_prev * float(input0[i0])));
        }
    }

    void chan_bb_a(Mult_state *state) {
        float fSlow0 = (float(*x1_samps) * float(*x2_samps));
        Sample x2_incr = (fSlow0 - state->x2_prev) * BL_RECIP;
        Sample x2_prev = state->x2_prev;
        state->x2_prev = fSlow0;
        Sample x1_incr = (fSlow0 - state->x1_prev) * BL_RECIP;
        Sample x1_prev = state->x1_prev;
        state->x1_prev = fSlow0;
        for (int i0 = 0; (i0 < BL); i0 = (i0 + 1)) {
            x2_prev += x2_incr;
            x1_prev += x1_incr;
            *out_samps++ = FAUSTFLOAT(x1_prev);
        }
    }

    void real_run() {
        x1_samps = x1->run(current_block); // update input
        x2_samps = x2->run(current_block); // update input
        Mult_state *state = &states[0];
        for (int i = 0; i < chans; i++) {
            (this->*run_channel)(state);
            state++;
            x1_samps += x1_stride;
            x2_samps += x2_stride;
        }
    }
};
#endif