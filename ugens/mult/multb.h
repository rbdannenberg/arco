/* multb -- unit generator for arco
 *
 * generated by f2a.py
 */

/*------------- BEGIN FAUST PREAMBLE -------------*/

/* ------------------------------------------------------------
name: "multb", "mult"
Code generated with Faust 2.37.3 (https://faust.grame.fr)
Compilation options: -lang cpp -os0 -light -es 1 -single -ftz 0
------------------------------------------------------------ */

#ifndef  __Multb_H__
#define  __Multb_H__

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

#include <algorithm>
#include <cmath>
#include <cstdint>


#ifndef FAUSTCLASS 
#define FAUSTCLASS Multb
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
#define FAUST_REAL_CONTROLS 1
/*-------------- END FAUST PREAMBLE --------------*/

extern const char *Multb_name;

class Multb : public Ugen {
public:
    struct Multb_state {
    };
    Vec<Multb_state> states;
    void (Multb::*run_channel)(Multb_state *state);

    Ugen_ptr x1;
    int x1_stride;
    Sample_ptr x1_samps;

    Ugen_ptr x2;
    int x2_stride;
    Sample_ptr x2_samps;


    Multb(int id, int nchans, Ugen_ptr x1_, Ugen_ptr x2_) : Ugen(id, 'b', nchans) {
        x1 = x1_;
        x2 = x2_;
        states.init(chans);


        // initialize channel states
        for (int i = 0; i < chans; i++) {

        }

        init_x1(x1);
        init_x2(x2);
    }

    ~Multb() {
        x1->unref();
        x2->unref();
    }

    const char *classname() { return Multb_name; }

    void print_sources(int indent, bool print) {
        x1->print_tree(indent, print, "x1");
        x2->print_tree(indent, print, "x2");
    }

    void repl_x1(Ugen_ptr inp) {
        x1->unref();
        init_x1(inp);
    }

    void repl_x2(Ugen_ptr inp) {
        x2->unref();
        init_x2(inp);
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

    void real_run() {
        x1_samps = x1->run(current_block); // update input
        x2_samps = x2->run(current_block); // update input
        Multb_state *state = &states[0];
        for (int i = 0; i < chans; i++) {
            *out_samps = FAUSTFLOAT(*x1_samps);
            state++;
            x1_samps += x1_stride;
            x2_samps += x2_stride;
        }
    }
};
#endif