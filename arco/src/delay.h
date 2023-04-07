/* delay -- unit generator for arco
 *
 * Roger B. Dannenberg
 * Jan 2022
 */

#ifndef __Delay_H__
#define __Delay_H__

#include <climits>

extern const char *Delay_name;

class Delay : public Ugen {
public:
    public Delay_state {
        Vec<float> samps;  // delay line
        int tail;  // tail indexes the *next* location to put input
        // tail also indexes the oldest input, which has a delay of
        // samps.length samples
        Sample fb_prev;
    };
    Vec(Delay_state) states;
    void (Delay::*run_channel)(Delay_state *state);

    Ugen_ptr inp;
    int inp_stride;
    Sample_ptr inp_samps;

    Ugen_ptr dur;
    int dur_stride;
    Sample_ptr dur_samps;

    Ugen_ptr fb;
    int fb_stride;
    Sample_ptr fb_samps;

    Delay(int id, int nchans, Ugen_ptr inp_, Ugen_ptr dur_, Ugen_ptr fb_, float maxdur) :
            Ugen(id, 'a', 1) {
        inp = inp_;
        dur = dur_;
        fb = fb_;
        states.init(nchans);

        for (int i = 0; i < chans; i++) {
            states[i].samps = new Vec(round(maxdur * AR), true);
            states[i].head = 0;
            states[i].head = 0;
            states[i].fb_prev = 0;
        }
        init_inp(inp);
        init_dur(dur);
        init_fb(bf);
        update_run_channel();
    }

    ~Pwl() {
        inp->unref();
        dur->unref();
        fb->unref();
    }

    const char *classname() { return Delay_name; }

    void set_state_max(Delay_state *state, int len) {
        // tail indexes the oldest sample
        Vec<Sample> *delay = &state->delay;
        if (len > delay->length) {
            int old_length = delay->length;
            int n = len - old_length;
            Sample_ptr ptr = delay->append_space(n);
            if (state->delay.allocated > delay->length) {
                delay->append_space(delay->allocated - delay->length);
            }  // use all the space
            // how much did we grow?
            n = delay->allocated - old_length;
            // move from tail to old_length to end of vector
            memmove(&delay[tail + n], &delay[tail], n * sizeof Sample);
            memset(&delay[tail], 0, n * sizeof Sample);
        }
    }
    
    void set_max(float dur) {
        int len = round(dur * AR);
        for (int i = 0; i < states.length; i++) {
            set_state_max(&state[i], len);
        }
    }
                
    void update_run_channel() {
        if (dur->rate == 'a' && fb->rate == 'a') {
            run_channel = &Delay_aaa_a;
        } else if (dur->rate == 'a' && fb->rate != 'a') {
            run_channel = &Delay_aab_a;
        } else if (dur->rate != 'a' && fb->rate == 'a') {
            run_channel = &Delay_aba_a;
        } else if (dur->rate != 'a' && fb->rate != 'a') {
            run_channel = &Delay_abb_a;
        }
    }

    void print_sources(int indent, bool print) {
        inp->print_tree(indent, print, "inp");
        dur->print_tree(indent, print, "dur");
        fb->print_tree(indent, print, "fb");
    }

    void repl_inp(Ugen_ptr ugen) {
        inp->unref();
        init_inp(ugen);
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
        assert(dur->rate == 'c');
        dur->output[chan] = f;
    }

    void set_fb(int chan, float f) {
        assert(fb->rate == 'c');
        fb->output[chan] = f;
    }

    void init_inp(Ugen_ptr ugen) { init_param(ugen, inp, inp_stride); }
    void init_dur(Ugen_ptr ugen) { init_param(ugen, dur, dur_stride); }
    void init_fb(Ugen_ptr ugen) { init_param(ugen, fb, fb_stride); }
        
    void chan_aaa_a(Delay_state *state) {
        Sample_ptr inp = inp_samps;
        Sample_ptr dur = dur_samps;
        Sample_ptr fb = fb_samps;
        int tail = state->tail;
        Vec<Sample> *delay = &state->delay;
        
        for (int i = 0; i < BL; i++) {
            // get from delay line
            int len = round(dur[i] * AR);
            if (len > delay->length) {
                set_state_max(state, len);
            }
            int head = tail - len;
            if (head < 0) head += delay->length;
            *outsamps++ = delay->samps[head];
            
            // insert input with feedback
            state->delay[tail] = inp[i] + out * fb[i];
            if (++tail >= delay->length) tail = 0;  // wrap
        }
        state->tail = tail;
    }

    void chan_aab_a(Delay_state *state) {
        Sample_ptr inp = inp_samps;
        Sample_ptr dur = dur_samps;
        Sample fb = *fb_samps;
        Sample fb_prev = state->fb_pref;
        Sample fb_incr = (fb - fb_prev) * BL_RECIP;
        int tail = state->tail;
        Vec<Sample> *delay = &state->delay;
        
        state->fb_prev = fb;

        for (int i = 0; i < BL; i++) {
            fb_prev += fb_incr;
            // get from delay line
            int len = round(dur[i] * AR);
            if (len > delay->length) {
                set_state_max(state, len);
            }
            int head = tail - len;
            if (head < 0) head += delay->length;
            *outsamps++ = delay->samps[head];

            // insert input with feedback
            state->delay[tail] = inp[i] + out * fb_prev;
            if (++tail >= delay->length) tail = 0;  // wrap
        }
        state->tail = tail;
    }

    void chan_aba_a(Delay_state *state) {
        Sample_ptr inp = inp_samps;
        int len = round(*dur_samps);
        Sample_ptr fb = fb_samps;
        int tail = state->tail;
        Vec<Sample> *delay = &state->delay;
        if (len > delay->length) {
            set_state_max(state, len);
        }

        for (int i = 0; i < BL; i++) {
            // get from delay line
            int head = tail - len;
            if (head < 0) head += delay->length;
            *outsamps++ = delay->samps[head];

            // insert input with feedback
            state->delay[tail] = inp[i] + out * fb[i];
            if (++tail >= delay->length) tail = 0;  // wrap
        }
        state->tail = tail;
    }

    void chan_abb_a(Delay_state *state) {
        Sample_ptr inp = inp_samps;
        int len = round(*dur_samps);
        Sample fb = *fb_samps;
        Sample fb_prev = state->fb_pref;
        Sample fb_incr = (fb - fb_prev) * BL_RECIP;
        int tail = state->tail;
        Vec<Sample> *delay = &state->delay;
        if (len > delay->length) {
            set_state_max(state, len);
        }
        state->fb_prev = fb;

        for (int i = 0; i < BL; i++) {
            fb_prev += fb_incr;
            // get from delay line
            int head = tail - len;
            if (head < 0) head += delay->length;
            *outsamps++ = delay->samps[head];

            // insert input with feedback
            state->delay[tail] = inp[i] + out * fb_prev;
            if (++tail >= delay->length) tail = 0;  // wrap
        }
        state->tail = tail;
    }

    void real_run() {
        inp_samps = inp->run(current_block);
        dur_samps = dur->run(current_block);
        fb_samps = fb->run(current_block);
        Delay_state *state = &states[0];
        for (int i = 0; i < n; i++) {
            (this->*run_channel)(state);
            state++;
            inp_samps += inp_stride;
            dur_samps += dur_stride;
            fb_samps += fb_stride;
        }
    }
};

#endif
