/* smoothb.h -- unit generator like const, but transitions are smoothed
 *
 * Roger B. Dannenberg
 * Oct 2023
 */

extern const char *Smoothb_name;

class Smoothb : public Ugen {
public:
    struct Smoothb_state {
        Sample target;
        Sample prev;
    };
    Vec<Smoothb_state> states;
    float cutoff;
    float alpha, one_minus_alpha;

    Smoothb(int id, int nchans, float cutoff = 10.0) :
            Ugen(id, 'b', nchans) {
        states.set_size(nchans, true);;
        for (int i = 0; i < nchans; i++) {
            output[i] = 0;
        }
        set_cutoff(cutoff);
    };

    ~Smoothb() { ; }

    
    const char *classname() { return Smoothb_name; }


    void print_details(int indent) {
        arco_print("cutoff %g targets [", cutoff);
        bool need_comma = false;
        for (int i = 0; i < chans; i++) {
            arco_print("%s%g", need_comma ? ", " : "", states[i].target);
            need_comma = true;
        }
        arco_print("]\n");
    }


    void real_run() {
        Smoothb_state *state = &states[0];
        for (int i = 0; i < chans; i++) {
            state->prev = alpha * state->target + one_minus_alpha * state->prev;
            *out_samps++ = state->prev;
            state++;
        }
    }


    void set_cutoff(float hz) {
        float k = 1 - cos(PI2 * hz * BP);
        alpha = -k + sqrt((2 + k) * k);
        one_minus_alpha = 1 - alpha;
    }


    void set_value(int chan, Sample value) {
        if (!output.bounds_check(chan)) {
            arco_warn("Smooth::set_value id %d chan %d but actual chans"
                      " is %d", id, chan, chans);
            return;
        }
        states[chan].target = value;
    }        
};
