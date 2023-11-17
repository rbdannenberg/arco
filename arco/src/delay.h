/* delay -- unit generator for arco
 *
 * Roger B. Dannenberg
 * Jan 2022
 */
/* Delay buffer (samps) is basically kept full of samples, a timespan of maxdur.
 * The delay is always at least 1 sample.
 * To compute the next sample:
 *   1. find where the head is based on delay time (dur).
 *   2. read from the head, multiply by feedback (fb), add input and
 *      store at tail
 *   3. increment head and tail
 * The tail points to where to write next, and head points to where to read
 * next. In the limit, both can point to the same location because we read
 * first. Thus a delay of 1 requires only 1 sample, and N requires N.
 * Initially, in case the delay is set to maxdur, we zero everything.
 * There is no head pointer, only a duration that tells how far to reach back.
 */
#ifndef __Delay_H__
#define __Delay_H__

#include <climits>
#include "ringbuf.h"

extern const char *Delay_name;

class Delay : public Ugen {
public:
    struct Delay_state {
        Ringbuf samps;  // delay line
        Dcblock dcblock;
        Sample fb_prev;
    };
    Vec<Delay_state> states;
    void (Delay::*run_channel)(Delay_state *state);

    Ugen_ptr input;
    int input_stride;
    Sample_ptr input_samps;

    Ugen_ptr dur;
    int dur_stride;
    Sample_ptr dur_samps;

    Ugen_ptr fb;
    int fb_stride;
    Sample_ptr fb_samps;

    Delay(int id, int nchans, Ugen_ptr input_, Ugen_ptr dur_,
	  Ugen_ptr fb_, float maxdur) : Ugen(id, 'a', nchans) {
        input = input_;
        dur = dur_;
        fb = fb_;
        states.set_size(nchans);

        for (int i = 0; i < chans; i++) {
            Ringbuf &delay = states[i].samps;
            delay.init((int) (maxdur * AR) + 1);  // round up
            // use all 2^n - 1 slots since delay is variable and might increase,
            // we want as much history as we can store in the allocated space:
            delay.set_fifo_len(delay.mask, true);
            states[i].fb_prev = 0;
            states[i].dcblock.init();
        }
        init_input(input);
        init_dur(dur);
        init_fb(fb);
        update_run_channel();
    }

    ~Delay() {
        input->unref();
        dur->unref();
        fb->unref();
        for (int i = 0; i < chans; i++) {
            states[i].samps.finish();
        }
    }

    const char *classname() { return Delay_name; }

    void set_state_max(Delay_state *state, int len) {
        // tail indexes the oldest sample
        Ringbuf &delay = state->samps;
        delay.set_fifo_len(len, true);
    }
    
    void set_max(float dur) {
        int len = round(dur * AR);
        for (int i = 0; i < states.size(); i++) {
            set_state_max(&states[i], len);
        }
    }
                
    void update_run_channel() {
        if (dur->rate == 'a' && fb->rate == 'a') {
            run_channel = &Delay::chan_aaa_a;
        } else if (dur->rate == 'a' && fb->rate != 'a') {
            run_channel = &Delay::chan_aab_a;
        } else if (dur->rate != 'a' && fb->rate == 'a') {
            run_channel = &Delay::chan_aba_a;
        } else if (dur->rate != 'a' && fb->rate != 'a') {
            run_channel = &Delay::chan_abb_a;
        }
    }

    void print_sources(int indent, bool print_flag) {
        input->print_tree(indent, print_flag, "input");
        dur->print_tree(indent, print_flag, "dur");
        fb->print_tree(indent, print_flag, "fb");
    }

    void repl_input(Ugen_ptr ugen) {
        input->unref();
        init_input(ugen);
        update_run_channel();
    }

    void repl_dur(Ugen_ptr ugen) {
        dur->unref();
        init_dur(ugen);
        update_run_channel();
    }

    void repl_fb(Ugen_ptr ugen) {
        fb->unref();
        init_fb(ugen);
        update_run_channel();
    }

    void set_dur(int chan, float f) {
        dur->const_set(chan, f, "Delay::set_dur");
    }

    void set_fb(int chan, float f) {
        fb->const_set(chan, f, "Delay::set_fb");
    }

    void init_input(Ugen_ptr ugen) { init_param(ugen, input, input_stride); }
    void init_dur(Ugen_ptr ugen) { init_param(ugen, dur, dur_stride); }
    void init_fb(Ugen_ptr ugen) { init_param(ugen, fb, fb_stride); }
        
    void chan_aaa_a(Delay_state *state) {
        Sample_ptr input = input_samps;
        Sample_ptr dur = dur_samps;
        Sample_ptr fb = fb_samps;
        Ringbuf &delay = state->samps;
        
        for (int i = 0; i < BL; i++) {
            // get from delay line
            int len = round(dur[i] * AR);
            if (len > delay.mask) {
                set_state_max(state, len);
            }
            Sample out = delay.get_nth(len);
            *out_samps++ = out;
            
            // insert input with feedback
            delay.toss(1);  // make space
            delay.enqueue(state->dcblock.filter(input[i] + out * fb[i]));
        }
    }

    void chan_aab_a(Delay_state *state) {
        Sample_ptr input = input_samps;
        Sample_ptr dur = dur_samps;
        Sample fb = *fb_samps;
        Sample fb_prev = state->fb_prev;
        Sample fb_incr = (fb - fb_prev) * BL_RECIP;
        Ringbuf &delay = state->samps;
        
        state->fb_prev = fb;

        for (int i = 0; i < BL; i++) {
            fb_prev += fb_incr;
            // get from delay line
            int len = round(dur[i] * AR);
            if (len > delay.size()) {
                set_state_max(state, len);
            }
            Sample out = delay.get_nth(len);
            *out_samps++ = out;

            // insert input with feedback
            delay.toss(1);  // make space
            delay.enqueue(state->dcblock.filter(input[i] + out * fb_prev));
        }
    }

    void chan_aba_a(Delay_state *state) {
        Sample_ptr input = input_samps;
        int len = round(*dur_samps * AR);
        Sample_ptr fb = fb_samps;
        Ringbuf &delay = state->samps;
        if (len > delay.size()) {
            set_state_max(state, len);
        }

        for (int i = 0; i < BL; i++) {
            // get from delay line
            Sample out = delay.get_nth(len);
            *out_samps++ = out;

            // insert input with feedback
            delay.toss(1);  // make space
            delay.enqueue(state->dcblock.filter(input[i] + out * fb[i]));
        }
    }

    void chan_abb_a(Delay_state *state) {
        Sample_ptr input = input_samps;
        int len = round(*dur_samps * AR);
        Sample fb = *fb_samps;
        Sample fb_prev = state->fb_prev;
        Sample fb_incr = (fb - fb_prev) * BL_RECIP;
        Ringbuf &delay = state->samps;
        if (len > delay.size()) {
            set_state_max(state, len);
        }
        state->fb_prev = fb;

        for (int i = 0; i < BL; i++) {
            fb_prev += fb_incr;
            // get from delay line
            Sample out = delay.get_nth(len);
            *out_samps++ = out;

            // insert input with feedback
            delay.toss(1);
            delay.enqueue(state->dcblock.filter(input[i] + out * fb_prev));
        }
    }

    void real_run() {
        input_samps = input->run(current_block);
        dur_samps = dur->run(current_block);
        fb_samps = fb->run(current_block);
        // delay does not terminate due to unknown tail_blocks
        Delay_state *state = &states[0];
        for (int i = 0; i < chans; i++) {
            (this->*run_channel)(state);
            state++;
            input_samps += input_stride;
            dur_samps += dur_stride;
            fb_samps += fb_stride;
        }
    }
};

#endif
