/* fader -- unit generator for arco
 *
 * Roger B. Dannenberg
 * Nov 2023
 *
 * fader combines a 1-segment envelope with a multiply to form 
 * a smooth gain control. This implementation is based on Multx.
 *
 */

#ifndef  __Fader_H__
#define  __Fader_H__

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

extern const char *Fader_name;
const int FADE_LINEAR = 0;
const int FADE_EXPONENTIAL = 1;
const int FADE_LOWPASS = 2;
const int FADE_SMOOTH = 3;


class Fader : public Ugen {
public:
    struct Fader_state {
        Sample current;
        Sample goal;
        Sample step;
        Sample delta;
        Sample factor;
        Sample phase;
    };
    Vec<Fader_state> states;

    Ugen_ptr input;
    int input_stride;
    Sample_ptr input_samps;

    void (Fader::*run_channel)(Fader_state *state);

    int mode;
    int dur_samps;
    int count;   // how many samples left in fade?

    Fader(int id, int nchans, Ugen_ptr input_, float current, int mode) :
            Ugen(id, 'a', nchans) {
        input = input_;
        set_mode(mode);
        set_dur(0.1);  // seconds
        states.set_size(chans);

        init_input(input);
        update_run_channel();
        // normally, Arco Ugens reinitialize channel states whenever
        // a new input has a different rate, changing the run_channel
        // method. In this special case, reverting to the starting
        // value for x2 (x2_init) after some computation makes no
        // sense, so we only initialize state here at the beginning.
        initialize_channel_states(current);
    }


    ~Fader() {
        input->unref();
    }


    const char *classname() { return Fader_name; }

    
    void initialize_channel_states(float current) {
        for (int i = 0; i < chans; i++) {
            set_current(i, current);
        }
    }


    void set_mode(int m) {
        mode = m;
        run_channel = &Fader::chan_linear;
        if (m == FADE_EXPONENTIAL) run_channel = &Fader::chan_exponential;
        else if (m == FADE_LOWPASS) run_channel = &Fader::chan_relaxation;
        else if (m == FADE_SMOOTH) run_channel = &Fader::chan_smooth_br;
        count = 0;
    }
    

    void update_run_channel() {
        // initialize based on input types
        if (input->rate != 'a') {
            input = new Upsample(-1, input->chans, input);
        }
    }


    void print_details(int indent) {
        arco_print("dur_samps %d remaining %d mode %d current",
                   dur_samps, count, mode);
        for (int i = 0; i < chans; i++) {
            arco_print(" %g", states[i].current);
        }
    }

    void print_sources(int indent, bool print_flag) {
        input->print_tree(indent, print_flag, "input");
    }


    void repl_input(Ugen_ptr ugen) {
        input->unref();
        init_input(ugen);
        update_run_channel();
    }


    void set_input(int chan, float f) {
        input->const_set(chan, f, "Fader::set_input");
    }


    void init_input(Ugen_ptr ugen) { init_param(ugen, input, &input_stride); }


    void set_current(int chan, Sample current) {
        Fader_state &state = states[chan];
        state.current = current;
        // prevent all modes from changing current:
        state.goal = current;
        state.delta = 0;
        state.factor = 1;
    }


    void set_dur(float d) {
        dur_samps = MAX(1, (int) (d * BR + 0.5));
    }


    void set_goal(int chan, Sample goal) {
        states[chan].goal = goal;
        if (chan == states.size() - 1) {  // set last channel, activate fade:
            count = dur_samps;
            for (int i = 0; i < chans; i++) {
                Fader_state &state = states[i];
                if (mode == FADE_EXPONENTIAL) {
                    state.factor = pow((double) (state.goal + 0.01) /
                                       (double) (state.current + 0.01),
                                              (double) 1.0 / dur_samps);
                } else if (mode == FADE_LOWPASS) {
                    state.factor = pow((double) 0.01,
                                       (double) 1.0 / dur_samps);
                    // make the delta 1% too big so that when we relax 99%
                    // of delta, we'll actually close the full gap to goal:
                    state.delta = (state.goal - state.current) * 1.01;
                } else if (mode == FADE_SMOOTH) {
                    // start where cos table is 1 and read it backwards
                    // to get a signal that diminishes to zero:
                    state.delta = float(-COS_TABLE_SIZE) / dur_samps;
                    state.factor = state.goal - state.current;
                    state.phase = 2 + COS_TABLE_SIZE;
                    if (dur_samps > 0.01 * BR) {  // longer than 10 msec
                        run_channel = &Fader::chan_smooth_br;
                    } else {
                        run_channel = &Fader::chan_smooth_ar;
                        // adding delta at audio rate, so it must be smaller:
                        state.delta *= BL_RECIP;
                    }
                } else {  // linear is the default
                    state.step = (state.goal - state.current) / dur_samps;
                }
            }
        }
    }


    void chan_linear(Fader_state *state) {
        Sample prev = state->current;
        state->current += state->step;
        Sample incr = (state->current - prev) * BL_RECIP;
        for (int i = 0; i < BL; i++) {
            prev += incr;
            *out_samps++ = input_samps[i] * prev;
        }
    }


    void chan_exponential(Fader_state *state) {
        Sample prev = state->current;
        state->current = (state->current + 0.01) * state->factor - 0.01;
        Sample incr = (state->current - prev) * BL_RECIP;
        for (int i = 0; i < BL; i++) {
            prev += incr;
            *out_samps++ = input_samps[i] * prev;
        }
    }


    void chan_relaxation(Fader_state *state) {
        Sample prev = state->current;
        state->current = state->goal - (state->delta *= state->factor);
        Sample incr = (state->current - prev) * BL_RECIP;
        for (int i = 0; i < BL; i++) {
            prev += incr;
            *out_samps++ = input_samps[i] * prev;
        }
    }


    // use delta to represent the phase increment
    // use factor for the scale factor (similar to delta in relaxation)
    // use phase as value from 2 to 102 (COS_TABLE_SIZE + 2)
    void chan_smooth_br(Fader_state *state) {
        Sample prev = state->current;
        state->phase += state->delta;
        int phasei = (int) state->phase;
        float rc = raised_cosine[phasei];
        // linear interpolation:
        rc += (raised_cosine[phasei] - rc) * (state->phase - phasei);
        state->current = state->goal - state->factor * rc;
        Sample incr = (state->current - prev) * BL_RECIP;
        for (int i = 0; i < BL; i++) {
            prev += incr;
            *out_samps++ = input_samps[i] * prev;
        }
    }


    // For very short (< 10 msec) duration, interpolate sample-by-sample:
    void chan_smooth_ar(Fader_state *state) {
        Sample cur = state->current;
        Sample goal = state->goal;
        float factor = state->factor;
        float phase = state->phase;
        for (int i = 0; i < BL; i++) {
            phase += state->delta;
            int phasei = (int) state->phase;
            float rc = raised_cosine[phasei];
            // linear interpolation:
            rc += (raised_cosine[phasei] - rc) * (phase - phasei);
            cur = goal - factor * rc;
            *out_samps++ = input_samps[i] * cur;
        }
        state->current = cur;
        state->phase = phase;
    }


    // when fade has ended, install this 
    void chan_static(Fader_state *state) {
        Sample gain = state->current;
        for (int i = 0; i < BL; i++) {
            *out_samps++ = input_samps[i] * gain;
        }
    }
        

    void real_run() {
        input_samps = input->run(current_block); // update input
        Fader_state *state = &states[0];
        if (count == 0) {
            run_channel = &Fader::chan_static;
            // if all goals are zero, we might terminate
            if (flags & CAN_TERMINATE) {
                bool all_zeros = true;
                for (int i = 0; i < chans; i++) {
                    all_zeros &= (state[i].goal == 0);
                }
                if (all_zeros) {
                    terminate();
                }
            }
        }
        count--;
        for (int i = 0; i < chans; i++) {
            (this->*run_channel)(state);
            state++;
            input_samps += input_stride;
        }
    }
};
#endif
