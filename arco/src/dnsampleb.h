/* dnsampleb -- unit generator for arco
 *
 * Roger B. Dannenberg
 * Oct 2023
 */

extern const char *Dnsampleb_name;

enum Dnsampleb_mode {
    BASIC = 0,
    AVG = 1,
    PEAK = 2,
    RMS = 3,
    POWER = 4,
    LOWPASS500 = 5,
    LOWPASS100 = 6
};
const int DS_MODE_LEN = 7;

class Dnsampleb;
class Dnsampleb_state;
typedef Sample (Dnsampleb::*Dnsampleb_method)(Dnsampleb_state *state);
extern Dnsampleb_method dnsampleb_methods[];
    

// Notes: -3dB cutoff frequencies are derived from:
//    y = alpha * input + one_minus_alpha * prev; prev = y;
//    where alpha = -k + sqrt(k*k + 2k), k = 1 - cos(cutoff),
//          cutoff = 2*Pi*f/r, f = cutoff frequency, r = sample rate
// For audio rate = AR = 44100, attenuation at the block-rate Nyquist
// frequency of 689 Hz should be approximately as follows:
//     LOWPASS500   -4 dB
//     LOWPASS100  -17 dB
// To get other cutoff frequencies, choose either LOWPASS500 or LOWPASS100
// and then invoke the method set_cutoff() to change the cutoff frequency.

// normally, we'd put this inside Dnsampleb, but we need to declare an
// array of method pointers that take Dnsampleb_state as parameter. A
// circular dependency is created if Dnsampleb_state cannot be referenced
// until Dnsampleb is fully specified.
struct Dnsampleb_state {
    Sample prev;
};

class Dnsampleb : public Ugen {
public:
    Vec<Dnsampleb_state> states;
    Dnsampleb_method dnsampleb;

    Ugen_ptr input;
    int32_t input_stride;
    Sample_ptr input_samps;

    float alpha, one_minus_alpha;

    Dnsampleb(int id, int nchans, Ugen_ptr input, int mode) :
            Ugen(id, 'a', nchans) {
        states.set_size(chans);

        // initialize channel states
        for (int i = 0; i < chans; i++) {
            states[i].prev = 0.0f;
        }
        init_input(input);
        set_mode(mode);
    }

    ~Dnsampleb() {
        input->unref();
    }


    const char *classname() { return Dnsampleb_name; }


    void print_sources(int indent, bool print_flag) {
        input->print_tree(indent, print_flag, "input");
    }


    void repl_input(Ugen_ptr input) {
        input->unref();
        init_input(input);
    }


    void set_input(int chan, float f) {
        input->const_set(chan, f, "Dnsampleb::set_input");
    }


    void init_input(Ugen_ptr ugen) { init_param(ugen, input, input_stride); }


    void set_cutoff(float hz) {
        float k = 1 - cos(PI2 * hz * AP);
        alpha = -k + sqrt((2 + k) * k);
        one_minus_alpha = 1 - alpha;
        // when input goes to zero, nth output sample is just
        // prev * one_minus_alpha**n. Let's conservatively go for -120 dB.
        // For tail time n samples, one_minus_alpha**n = log(0.0000001)
        // -> n * log(one_minus_alpha) = log(0.0000001)
        // -> n = log(0.0000001) / log(one_minus_alpha)
        tail_blocks = (int) (log(0.0000001) / (log(one_minus_alpha) * BL));
    }
    

    void set_mode(int mode) {
        if (mode < 0 || mode >= DS_MODE_LEN) {
            arco_warn("Dnsampleb set_mode got invalid mode, using BASIC");
            mode = 0;
        }
        dnsampleb = dnsampleb_methods[mode];
        tail_blocks = 0;
        if (mode == (int) LOWPASS500) {
            set_cutoff(500.0);
        } else if (mode == (int) LOWPASS100) {
            set_cutoff(100.0);
        }
    }
    

    Sample dnsampleb_basic(Dnsampleb_state *state) {
        Sample s = *input_samps;
        return s;
    }

    Sample dnsampleb_avg(Dnsampleb_state *state) {
        Sample s = 0;
        for (int i = 0; i < BL; i++) {
            s += input_samps[i];
        }
        return s * BL_RECIP;
    }

    Sample dnsampleb_peak(Dnsampleb_state *state) {
        Sample s = fabs(*input_samps++);
        for (int i = 1; i < BL; i++) {
            s = MAX(fabs(input_samps[i]), s);
        }
        return s;
    }

    Sample dnsampleb_power(Dnsampleb_state *state) {
        Sample sum = 0;
        for (int i = 0; i < BL; i++) {
            Sample s = input_samps[i];
            sum += s * s;
        }
        return sum * BL_RECIP;
    }

    Sample dnsampleb_rms(Dnsampleb_state *state) {
        return sqrt(dnsampleb_power(state));
    }

    Sample dnsampleb_lowpass(Dnsampleb_state *state) {
        Sample sum = 0;
        for (int i = 0; i < BL; i++) {
            state->prev = alpha * input_samps[i] +
                          one_minus_alpha * state->prev;
        }
        return state->prev;
    }


    void real_run() {
        input_samps = input->run(current_block); // update input
        if (input->flags & TERMINATED) {
            terminate();
        }
        Dnsampleb_state *state = &states[0];
        for (int i = 0; i < chans; i++) {
            *out_samps++ = (this->*dnsampleb)(state++);
            input_samps += input_stride;
        }
    }
};
