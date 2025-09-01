/* fader -- unit generator for arco
 *
 * Roger B. Dannenberg
 * Nov 2023
 *
 * fader combines a 1-segment envelope with a multiply to form 
 * a smooth gain control. This implementation is based on Multx.
 *
 * fade control is shared across all channels
 *    each channel maintains its own state so that channels can
 *        have different goals, although duration is shared and
 *        fades are synchronous (start/end at the same times).
 *    when all fades complete, the channel method is replaced by
 *        chan_static() which just multiplies each channel by
 *        its gain
 *    initially and normally, fading is bypassed by running
 *        chan_static(). Calling set_mode() starts a fade which
 *        runs to completion and reverts to chan_static(). The
 *        goals can be reset during a fade. New goals take effect
 *        simultaneously when set_mode() is called.
 *    when a fade starts, count is set to duration in blocks and
 *        counts down. When count == 0, chan_static() is installed.
 */

#ifndef  __Fader_H__
#define  __Fader_H__

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

extern const char *Fader_name;

class Fader : public Ugen {
public:
    struct Fader_state {
        Sample current;  // current value after each real-run
        Sample goal;     // desired end value after fade, changeable during fade
        Sample delta;    // phase increment, linear step size, or lowpass offset
        Sample factor;   // exponential/lowpass factor or goal - current
        Sample base;     // value based on goal or goal - current
        Sample phase;    // cosine phase
    };
    Vec<Fader_state> states;

    Ugen_ptr input;
    int input_stride;
    Sample_ptr input_samps;

    void (Fader::*run_channel)(Fader_state *state);

    int mode;
    int dur_samps;  // initial count, duration in blocks
    int count;   // how many samples left in fade?

    Fader(int id, int nchans, Ugen_ptr input_, float current) :
            Ugen(id, 'a', nchans) {
        input = input_;
        mode = FADE_LINEAR;  // unused, but at least let's make it valid
        set_dur(0.1);  // seconds
        states.set_size(chans);
        init_input(input);
        update_run_channel();
        // this also sets count
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
        switch (m) {
            case FADE_LINEAR:
            default:
                run_channel = &Fader::chan_linear;
                for (int i = 0; i < chans; i++) {
                    Fader_state &state = states[i];
                    state.delta = (state.goal - state.current) / dur_samps;
                }
                break;
            case FADE_EXPONENTIAL:
                run_channel = &Fader::chan_exponential;
                for (int i = 0; i < chans; i++) {
                    Fader_state &state = states[i];
                    // we have dur_stamps (n) steps, we want
                    // (current + 0.01) * factor^n == (goal + 0.01):
                    state.factor = pow((double) (state.goal + 0.01) /
                                       (double) (state.current + 0.01),
                                              (double) 1.0 / dur_samps);
                }
                break;
            case FADE_LOWPASS:
                run_channel = &Fader::chan_lowpass;
                for (int i = 0; i < chans; i++) {
                    Fader_state &state = states[i];
                    // Let g = gap from current to goal. We want:
                    //     goal - g * (1.01 * factor^0 - 0.01) == current
                    //     goal - g * (1.01 * factor^n - 0.01) == 
                    //         goal - g * (0.01 - 0.01) == goal
                    // So factor^n == 0.01 / 1.01
                    // In implementation, we need 
                    //     goal - (goal - current) * (1.01 * factor^i - 0.01) ==
                    //     goal + (goal - current) * 0.01 - 
                    //            (goal - current) * 1.01 * factor^i
                    // so we could take as base:
                    //     goal + goal * 0.01 - current * 0.01 == 
                    //         goal * 1.01 - current * 0.01
                    // and initial delta == current * 1.01 - goal * 1.01
                    //                   == (current - goal) * 1.01
                    // and each time, we set delta *= factor and
                    //     return base + delta
                    state.base = state.goal * 1.01 - state.current * 0.01;
                    state.delta = (state.current - state.goal) * 1.01;
                    state.factor = pow(0.01 / 1.01, (double) 1.0 / dur_samps);
                }
                break;
            case FADE_SMOOTH:
                if (dur_samps > 0.01 * BR) {  // longer than 10 msec
                    run_channel = &Fader::chan_smooth_br;
                } else {
                    run_channel = &Fader::chan_smooth_ar;
                }
                for (int i = 0; i < chans; i++) {
                    Fader_state &state = states[i];
                    // start where cos table is 1 and read it backwards
                    // to get a signal that diminishes to zero:
                    state.delta = float(-COS_TABLE_SIZE) / dur_samps;
                    state.factor = state.goal - state.current;
                    state.base = state.goal;
                    state.phase = 2 + COS_TABLE_SIZE;
                    // calculation: return base + factor * COSINE of phase
                    if (run_channel == &Fader::chan_smooth_ar) {
                        // adding delta at audio rate, so it must be smaller:
                        state.delta *= BL_RECIP;
                    }                    
                }
                break;
        }
        count = dur_samps;
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


    void init_input(Ugen_ptr ugen) {
        assert(ugen->rate == 'a');
        init_param(ugen, input, &input_stride);
    }


    void set_current(int chan, Sample current) {
        Fader_state &state = states[chan];
        state.current = current;
        count = -1;  // prevents modes from changing current:
        run_channel = &Fader::chan_static;
    }


    void set_dur(float d) {
        dur_samps = MAX(1, (int) (d * BR + 0.5));
    }


    void set_goal(int chan, Sample goal) {
        states[chan].goal = goal;
    }


    void chan_linear(Fader_state *state) {
        Sample prev = state->current;
        state->current += state->delta;
        Sample incr = (state->current - prev) * BL_RECIP;
        for (int i = 0; i < BL; i++) {
            prev += incr;
            *out_samps++ = input_samps[i] * prev;
        }
    }


    void chan_exponential(Fader_state *state) {
        Sample prev = state->current;
        state->current = (prev + 0.01) * state->factor - 0.01;
        Sample incr = (state->current - prev) * BL_RECIP;
        for (int i = 0; i < BL; i++) {
            prev += incr;
            *out_samps++ = input_samps[i] * prev;
        }
    }


    void chan_lowpass(Fader_state *state) {
        // see set_mode() for details on the calculation here
        Sample prev = state->current;
        state->current = state->base + (state->delta *= state->factor);
        Sample incr = (state->current - prev) * BL_RECIP;
        for (int i = 0; i < BL; i++) {
            prev += incr;
            *out_samps++ = input_samps[i] * prev;
        }
    }


    // use delta to represent the phase increment
    // use factor for the scale factor (similar to delta in lowpass)
    // use phase as value from 2 to 102 (COS_TABLE_SIZE + 2)
    void chan_smooth_br(Fader_state *state) {
        Sample cur = state->current;
        state->phase += state->delta;
        int phasei = (int) state->phase;
        float rc = raised_cosine[phasei];
        // linear interpolation:
        rc += (raised_cosine[phasei + 1] - rc) * (state->phase - phasei);
        state->current = state->base - state->factor * rc;
        Sample incr = (state->current - cur) * BL_RECIP;
        for (int i = 0; i < BL; i++) {
            cur += incr;
            *out_samps++ = input_samps[i] * cur;
        }
    }


    // For very short (< 10 msec) duration, interpolate sample-by-sample:
    void chan_smooth_ar(Fader_state *state) {
        float phase = state->phase;
        Sample cur;
        for (int i = 0; i < BL; i++) {
            phase += state->delta;
            int phasei = (int) phase;
            float rc = raised_cosine[phasei];
            // linear interpolation:
            rc += (raised_cosine[phasei + 1] - rc) * (phase - phasei);
            cur = state->base - state->factor * rc;
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
                    flags = TERMINATING;  // happens once
                }
            } else {
                send_action_id(ACTION_EVENT);
            }
        }
        count--;
        for (int i = 0; i < chans; i++) {
            (this->*run_channel)(state);
            state++;
            input_samps += input_stride;
        }
        if (flags & TERMINATING) {
            terminate(ACTION_EVENT | ACTION_END);
        }
    }
};
#endif
