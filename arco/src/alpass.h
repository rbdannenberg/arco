/* alpass -- unit generator for arco
 *
 * Roger B. Dannenberg
 * Jan 2022
 */
/* Alpass buffer (samps) is basically kept full of samples, a timespan of maxdur.
 * The alpass is always at least 1 sample.
 * To compute the next sample:
 *   1. find where the head is based on alpass time (dur).
 *   2. read from the head, multiply by feedback (fb), add input and
 *      store at tail
 *   3. increment head and tail
 * The tail points to where to write next, and head points to where to read
 * next. In the limit, both can point to the same location because we read
 * first. Thus a alpass of 1 requires only 1 sample, and N requires N.
 * Initially, in case the alpass is set to maxdur, we zero everything.
 * There is no head pointer, only a duration that tells how far to reach back.
 */
#ifndef __Alpass_H__
#define __Alpass_H__

#include <climits>

extern const char *Alpass_name;

class Alpass : public Ugen {
public:
    struct Alpass_state {
        Vec<float> samps;  // alpass line
        int tail;  // tail indexes the *next* location to put input
        // tail also indexes the oldest input, which has a alpass of
        // samps.size() samples
        Sample fb_prev;
    };
    Vec<Alpass_state> states;
    void (Alpass::*run_channel)(Alpass_state *state);

    Ugen_ptr inp;
    int inp_stride;
    Sample_ptr inp_samps;

    Ugen_ptr dur;
    int dur_stride;
    Sample_ptr dur_samps;

    Ugen_ptr fb;
    int fb_stride;
    Sample_ptr fb_samps;

    Alpass(int id, int nchans, Ugen_ptr inp_, Ugen_ptr dur_,
	  Ugen_ptr fb_, float maxdur) : Ugen(id, 'a', nchans) {
        inp = inp_;
        dur = dur_;
        fb = fb_;
        states.init(nchans);

        for (int i = 0; i < chans; i++) {
            Vec<Sample> *alpass = &states[i].samps;
            alpass->init((int) (maxdur * AR) + 1);  // round up
            alpass->set_size(alpass->get_allocated());
            alpass->zero();
            states[i].tail = 0;
            states[i].fb_prev = 0;
        }
        init_inp(inp);
        init_dur(dur);
        init_fb(fb);
        update_run_channel();
    }

    ~Alpass() {
        inp->unref();
        dur->unref();
        fb->unref();
        for (int i = 0; i < chans; i++) {
            states[i].samps.finish();
        }
    }

    const char *classname() { return Alpass_name; }

    void set_state_max(Alpass_state *state, int len) {
        // tail indexes the oldest sample
        Vec<Sample> &alpass = state->samps;
        int tail = state->tail;
        if (len > alpass.size()) {
            int old_length = alpass.size();
            Sample_ptr ptr = alpass.append_space(len - old_length);
            alpass.set_size(alpass.get_allocated());  // use all the space
            // how much did we grow?
            int n = alpass.size() - old_length;
            // how many samples from tail to the end of the old buffer?
            int m = (tail == 0 ? 0 : old_length - tail);
            // we had [tuvwxTpqrs], where x is the most recent sample, T
            // represents tail pointer and oldest sample, and so Tpqrs are
            // the oldest. When the buffer is extended, we get:
            // [tuvwxTpqrs???????????????] and we want to convert it to
            // [tuvwxT00000000000000opqrs] where o was the value at tail T.
            // opqrs has length m. ??????????????? has length n.
            memmove(&alpass[tail + n], &alpass[tail], n * sizeof(Sample));
            memset(&alpass[tail], 0, n * sizeof(Sample));
        }
    }
    
    void set_max(float dur) {
        int len = round(dur * AR);
        for (int i = 0; i < states.size(); i++) {
            set_state_max(&states[i], len);
        }
    }
                
    void update_run_channel() {
        if (dur->rate == 'a' && fb->rate == 'a') {
            run_channel = &Alpass::chan_aaa_a;
        } else if (dur->rate == 'a' && fb->rate != 'a') {
            run_channel = &Alpass::chan_aab_a;
        } else if (dur->rate != 'a' && fb->rate == 'a') {
            run_channel = &Alpass::chan_aba_a;
        } else if (dur->rate != 'a' && fb->rate != 'a') {
            run_channel = &Alpass::chan_abb_a;
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
        
    void chan_aaa_a(Alpass_state *state) {
        Sample_ptr inp = inp_samps;
        Sample_ptr dur = dur_samps;
        Sample_ptr fb = fb_samps;
        int tail = state->tail;
        Vec<Sample> &alpass = state->samps;
        
        for (int i = 0; i < BL; i++) {
            // get from alpass line
            int len = round(dur[i] * AR);
            if (len > alpass.size()) {
                set_state_max(state, len);
            }
            int head = tail - len;
            if (head < 0) head += alpass.size();
            Sample feedback = fb[i];
            Sample y = alpass[head];
            Sample z = feedback * y + inp[i];
            *out_samps++ = y - feedback * z;
            
            // insert input with feedback
            alpass[tail] = z;
            if (++tail >= alpass.size()) tail = 0;  // wrap
        }
        state->tail = tail;
    }

    void chan_aab_a(Alpass_state *state) {
        Sample_ptr inp = inp_samps;
        Sample_ptr dur = dur_samps;
        Sample fb = *fb_samps;
        Sample fb_prev = state->fb_prev;
        Sample fb_incr = (fb - fb_prev) * BL_RECIP;
        int tail = state->tail;
        Vec<Sample> &alpass = state->samps;
        
        state->fb_prev = fb;

        for (int i = 0; i < BL; i++) {
            fb_prev += fb_incr;
            // get from alpass line
            int len = round(dur[i] * AR);
            if (len > alpass.size()) {
                set_state_max(state, len);
            }
            int head = tail - len;
            if (head < 0) head += alpass.size();
            Sample y = alpass[head];
            Sample z = fb_prev * y + inp[i];
            *out_samps++ = y - fb_prev * z;

            // insert input with feedback
            alpass[tail] = z;
            if (++tail >= alpass.size()) tail = 0;  // wrap
        }
        state->tail = tail;
    }

    void chan_aba_a(Alpass_state *state) {
        Sample_ptr inp = inp_samps;
        int len = round(*dur_samps * AR);
        Sample_ptr fb = fb_samps;
        int tail = state->tail;
        Vec<Sample> &alpass = state->samps;
        if (len > alpass.size()) {
            set_state_max(state, len);
        }

        for (int i = 0; i < BL; i++) {
            // get from alpass line
            int head = tail - len;
            if (head < 0) head += alpass.size();
            Sample feedback = fb[i];
            Sample y = alpass[head];
            Sample z = feedback * y + inp[i];
            *out_samps++ = y - feedback * z;
            
            // insert input with feedback
            alpass[tail] = z;
            if (++tail >= alpass.size()) tail = 0;  // wrap
        }
        state->tail = tail;
    }

    void chan_abb_a(Alpass_state *state) {
        Sample_ptr inp = inp_samps;
        int len = round(*dur_samps * AR);
        Sample fb = *fb_samps;
        Sample fb_prev = state->fb_prev;
        Sample fb_incr = (fb - fb_prev) * BL_RECIP;
        int tail = state->tail;
        Vec<Sample> &alpass = state->samps;
        if (len > alpass.size()) {
            set_state_max(state, len);
        }
        state->fb_prev = fb;

        for (int i = 0; i < BL; i++) {
            fb_prev += fb_incr;
            // get from alpass line
            int head = tail - len;
            if (head < 0) head += alpass.size();
            Sample y = alpass[head];
            Sample z = fb_prev * y + inp[i];
            *out_samps++ = y - fb_prev * z;

            // insert input with feedback
            alpass[tail] = z;
            if (++tail >= alpass.size()) tail = 0;  // wrap
        }
        state->tail = tail;
    }

    void real_run() {
        inp_samps = inp->run(current_block);
        dur_samps = dur->run(current_block);
        fb_samps = fb->run(current_block);
        Alpass_state *state = &states[0];
        for (int i = 0; i < chans; i++) {
            (this->*run_channel)(state);
            state++;
            inp_samps += inp_stride;
            dur_samps += dur_stride;
            fb_samps += fb_stride;
        }
    }
};

#endif
