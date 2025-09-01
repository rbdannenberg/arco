/* smooth.h -- unit generator like const, but transitions are smoothed
 *
 * Roger B. Dannenberg
 * Aug 2025, adapted from smoothb
 */

extern const char *Smooth_name;

class Smooth : public Ugen {
public:
    struct Smooth_state {
        Sample target;
        Sample prev;
    };
    Vec<Smooth_state> states;
    float cutoff;
    float alpha, one_minus_alpha;

    Smooth(int id, int nchans, float cutoff = 10.0) :
            Ugen(id, 'b', nchans) {
        states.set_size(nchans, true);;
        for (int i = 0; i < nchans; i++) {
            output[i] = 0;
        }
        set_cutoff(cutoff);
    };

    ~Smooth() { ; }

    
    const char *classname() { return Smooth_name; }


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
        Smooth_state *state = &states[0];
        for (int ch = 0; ch < chans; ch++) {
            for (int i = 0; i < BL; i++) {
                state->prev = alpha * state->target + 
                              one_minus_alpha * state->prev;
                *out_samps++ = state->prev;
            }
            state++;
        }
    }


    void set_cutoff(float hz) {
        float k = 1 - cos(PI2 * hz * AP);
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
