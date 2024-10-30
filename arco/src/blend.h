/* blend -- unit generator for arco
 *
 * adapted from multx, Oct 2024
 *
 * blend is a selector using a signal to select or mix 2 inputs
 *
 * The blend parameter can be provided with an initial value like multx.
 * 
 * To cut down on the combinatorics of different rates, if only one of
 * x1 or x2 is audio rate, we swap them. In that case, the blend 
 * parameter b must become 1 - b, which is indicated by swap_x1_x2.
 */

#ifndef  __Blend_H__
#define  __Blend_H__

extern const char *Blend_name;

class Blend : public Ugen {
public:
    struct Blend_state {
        Sample prev_x1_gain;
        Sample prev_x2_gain;
        Sample prev_b;  // blend
    };
    Vec<Blend_state> states;
    void (Blend::*run_channel)(Blend_state *state);

    Ugen_ptr x1;
    int x1_stride;
    Sample_ptr x1_samps;

    Ugen_ptr x2;
    int x2_stride;
    Sample_ptr x2_samps;

    Ugen_ptr b;
    int b_stride;
    Sample_ptr b_samps;

    float gain;
    int mode;

    Blend(int id, int nchans, Ugen_ptr x1_, Ugen_ptr x2_, Ugen_ptr b_, 
          float b_init, int mode_) :
            Ugen(id, 'a', nchans) {
        x1 = x1_;
        x2 = x2_;
        b = b_;
        mode = mode_;
        gain = 1;
        flags = CAN_TERMINATE;
        states.set_size(chans);  // zeros everything

        init_x1(x1);
        init_x2(x2);
        init_b(b);
        update_run_channel();
        // normally, Arco Ugens reinitialize channel states whenever
        // a new input has a different rate, changing the run_channel
        // method. In this special case, reverting to the starting
        // value for x2 (x2_init) after some computation makes no
        // sense, so we only initialize state here at the beginning.
        initialize_channel_states(b_init);
    }

    ~Blend() {
        x1->unref();
        x2->unref();
        b->unref();
    }

    const char *classname() { return Blend_name; }

    void initialize_channel_states(float b_init) {
        for (int i = 0; i < chans; i++) {
            states[i].prev_b = b_init;
        }
    }

    void update_run_channel() {
        if (mode == BLEND_POWER) {
            run_channel = &Blend::power_aab_a;
        } else if (mode == BLEND_45) {
            run_channel = &Blend::p45_aab_a;
        } else {
            run_channel = &Blend::linear_aab_a;
        }
    }


    void print_sources(int indent, bool print_flag) {
        x1->print_tree(indent, print_flag, "x1");
        x2->print_tree(indent, print_flag, "x2");
        b->print_tree(indent, print_flag, "b");
    }

    void repl_x1(Ugen_ptr ugen) {
        x1->unref();
        init_x1(ugen);
        update_run_channel();
    }

    void repl_x2(Ugen_ptr ugen) {
        x2->unref();
        init_x2(ugen);
        update_run_channel();
    }

    void repl_b(Ugen_ptr ugen) {
        b->unref();
        init_b(ugen);
        update_run_channel();
    }

    void set_x1(int chan, float f) {
        x1->const_set(chan, f, "Blend::set_x1");
    }

    void set_x2(int chan, float f) {
        x2->const_set(chan, f, "Blend::set_x2");
    }

    void set_b(int chan, float f) {
        b->const_set(chan, f, "Blend::set_x2");
    }

    void init_x1(Ugen_ptr ugen) {
        if (ugen->rate != 'a') {
            ugen = new Upsample(-1, ugen->chans, ugen);
        }
        init_param(ugen, x1, &x1_stride); 
    }

    void init_x2(Ugen_ptr ugen) {
        if (ugen->rate != 'a') {
            ugen = new Upsample(-1, ugen->chans, ugen);
        }
        init_param(ugen, x2, &x2_stride);
    }

    void init_b(Ugen_ptr ugen) {
        if (ugen->rate == 'a') {
            ugen = new Dnsampleb(-1, ugen->chans, ugen, LOWPASS500);
        }
        init_param(ugen, b, &b_stride); 
    }

    
#define LINEAR_BLEND(out, x1, x2, b) \
            (out) = gain * ((x1) * (1.0f - (b)) + (x2) * (b))

#define COMPUTE_COS_SIN(b) \
    float angle = (COS_TABLE_SIZE + 1) - ((b) * (COS_TABLE_SIZE / 2.0)); \
    int anglei = (int) angle; \
    float x1_gain = raised_cosine[anglei]; \
    x1_gain += (angle - anglei) * (x1_gain - raised_cosine[anglei + 1]); \
    x1_gain += x1_gain - 1;  /* convert raised cos to cos */ \
    angle = (COS_TABLE_SIZE * 3 / 2.0f) - angle; \
    anglei = (int) angle; \
    float x2_gain = raised_cosine[anglei]; \
    x2_gain += (angle - anglei) * (x2_gain - raised_cosine[anglei + 1]); \
    x2_gain += x2_gain - 1;

#define COMPUTE_X1_X2_GAIN \
    Sample x1_gain_fast = state->prev_x1_gain; \
    Sample x1_gain_incr = (x1_gain - x1_gain_fast) * BL_RECIP; \
    state->prev_x1_gain = x1_gain; \
    Sample x2_gain_fast = state->prev_x2_gain; \
    Sample x2_gain_incr = (x2_gain - x2_gain_fast) * BL_RECIP; \
    state->prev_x2_gain = x2_gain;

#define P45_BLEND \
    x1_gain = sqrt((1 - b) * x1_gain);  /* blend linear with constant power */ \
    x2_gain = sqrt((1 - b) * x2_gain);  /* blend linear with constant power */ \

#define APPLY_X1_X2_GAIN \
    for (int i = 0; i < BL; i++) { \
        x1_gain_fast += x1_gain_incr; \
        x2_gain_fast += x2_gain_incr; \
        *out_samps++ = gain * (x1_samps[i] * x1_gain_fast + \
                               x2_samps[i] * x2_gain_fast); \
    }


    void linear_aab_a(Blend_state *state) {
        float b = *b_samps;
        Sample b_fast = state->prev_b;
        Sample b_incr = (b - b_fast) * BL_RECIP;
        state->prev_b = b;
        for (int i = 0; i < BL; i++) {
            b_fast += b_incr;
            *out_samps++ = gain * (x1_samps[i] * (1.0f - b_fast) +
                                   x2_samps[i] * b_fast);
        }
    }

    void power_aab_a(Blend_state *state) {
        float b = *b_samps;
        COMPUTE_COS_SIN(b);  // compute cos and sin terms from b
        COMPUTE_X1_X2_GAIN  // prepare to upsample x1_gain, x2_gain
        APPLY_X1_X2_GAIN  // compute output using x1_gain, x2_gain weighting
    }

    void p45_aab_a(Blend_state *state) {
        float b = *b_samps;
        COMPUTE_COS_SIN(b);  // compute cos and sin terms from b
        P45_BLEND  // blend const power gains with linear gains
        COMPUTE_X1_X2_GAIN  // prepare to upsample x1_gain, x2_gain
        APPLY_X1_X2_GAIN  // compute output using x1_gain, x2_gain weighting
    }


    void real_run() {
        x1_samps = x1->run(current_block); // update input
        x2_samps = x2->run(current_block); // update input
        b_samps = b->run(current_block);  // update input
        if ((x1->flags & x2->flags & TERMINATED) &&
            (flags & CAN_TERMINATE)) {
            terminate(ACTION_TERM);
        }
        Blend_state *state = &states[0];
        for (int i = 0; i < chans; i++) {
            (this->*run_channel)(state);
            state++;
            x1_samps += x1_stride;
            x2_samps += x2_stride;
            b_samps += b_stride;
        }
    }
};
#endif
