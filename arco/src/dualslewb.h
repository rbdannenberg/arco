/* dualslewb -- unit generator for arco
 *
 * Roger B. Dannenberg
 * Jan 2022
 *
 * portamento with independent attack/release rates and exponential or linear
 * attack/release
 */

#ifndef __Dualslewb_H__
#define __Dualslewb_H__

extern const char *Dualslewb_name;

const float BIAS = 0.01;

class Dualslewb : public Ugen {
public:
    struct Dualslewb_state {
        Sample current;  // current value
    };
    float attack;   // attack time in seconds
    float release;  // release time in seconds
    float attincr;
    float reldecr;  // slope (negative) or scale factor (< 1)
    bool attack_linear;
    bool release_linear;

    Vec<Dualslewb_state> states;
    void (Dualslewb::*run_channel)(Dualslewb_state *state);

    Ugen_ptr input;
    int input_stride;
    Sample_ptr input_samps;

    Dualslewb(int id, int nchans, Ugen_ptr input_, float attack, float release,
            Sample current, bool attack_linear, bool release_linear) :
	  Ugen(id, 'b', nchans) {
        set_attack(attack, attack_linear);
        set_release(release, release_linear);
        for (int i = 0; i < chans; i++) {
            set_current(current, i);
        }
        input = NULL;
        init_input(input_);
    }

    ~Dualslewb() {
        input->unref();
    }

    const char *classname() { return Dualslewb_name; }


    void print_details(int indent) {
        arco_print("attack %g (%s) release %g (%s)", attack,
                   attack_linear ? "lin" : "exp", release,
                   release_linear ? "lin" : "exp");
    }

    
    void set_current(Sample x, int chan) {
        states[chan].current = MAX(x, 0) + BIAS;
    }


    void set_attack(float attack_, bool attack_linear_) {
        attack = MAX(BP, attack_);  // at least 1 block period
        attack_linear = attack_linear_;
        if (attack_linear) {
            attincr = 1.0 / (attack * BR);
        } else {
            attincr = exp(log(1.0 / BIAS) / (attack * BR));
        }
    }
    
    void set_release(float release_, bool release_linear_) {
        release = MAX(BP, release_);  // at least 1 block period
        release_linear = release_linear_;
        if (release_linear) {
            reldecr = 1.0 / (release * BR);
        } else {
            reldecr = exp(log(BIAS) / (release * BR));
        }
    }

    void print_sources(int indent, bool print_flag) {
        input->print_tree(indent, print_flag, "inp");
    }

    void repl_input(Ugen_ptr ugen) {
        input->unref();
        init_input(ugen);
    }

    void init_input(Ugen_ptr ugen) {
        if (ugen->rate == 'a') {
            if (!input) {
                ugen = ugen_table[ZEROB_ID];
                arco_warn("Dualslewb does not allow audio-rate input. "
                          "Initializing input to zerob.");
            } else {
                arco_warn("Dualslewb ignoring attempt to replace input "
                          "with audio-rate signal.");
                return;
            }
        }
        init_param(ugen, input, input_stride);
    }

        
    void chan_b_b(Dualslewb_state *state) {
        Sample input = MAX(*input_samps, 0) + BIAS;
        Sample current = state->current;
        if (input > current) {  // slew up
            if (attack_linear) {
                current += attincr;
            } else {
                current *= attincr;
            }
            current = MIN(input, current);
        } else {
            if (attack_linear) {
                current += reldecr;
            } else {
                current *= reldecr;
            }
            current = MAX(input, current);
        }
        state->current = current;
        *out_samps = current - BIAS;
    }


    void real_run() {
        input_samps = input->run(current_block);
        Dualslewb_state *state = &states[0];
        for (int i = 0; i < chans; i++) {
            chan_b_b(state);
            state++;
            input_samps += input_stride;
        }
    }
};

#endif
