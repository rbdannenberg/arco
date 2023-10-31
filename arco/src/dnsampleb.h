/* dnsampleb -- unit generator for arco
 *
 * Roger B. Dannenberg
 * Oct 2023
 */

extern const char *Dnsampleb_name;

enum Dnsample_mode {
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
    Dnsampleb_method dnsample;

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
    }
    
    void set_mode(int mode) {
        if (mode < 0 || mode >= DS_MODE_LEN) {
            arco_warn("Dnsampleb set_mode got invalid mode, using BASIC");
            mode = 0;
        }
        dnsample = dnsampleb_methods[mode];
        if (mode == (int) LOWPASS500) {
            set_cutoff(500.0);
        } else if (mode == (int) LOWPASS100) {
            set_cutoff(100.0);
        }
    }
    

    Sample dnsample_basic(Dnsampleb_state *state) {
        Sample s = *input_samps;
        input_samps += BL;
        return s;
    }

    Sample dnsample_avg(Dnsampleb_state *state) {
        Sample s = 0;
        for (int i = 0; i < BL; i++) {
            s += *input_samps++;
        }
        return s * BL_RECIP;
    }

    Sample dnsample_peak(Dnsampleb_state *state) {
        Sample s = fabs(*input_samps++);
        for (int i = 1; i < BL; i++) {
            s = MAX(fabs(*input_samps++), s);
        }
        return s;
    }

    Sample dnsample_power(Dnsampleb_state *state) {
        Sample sum = 0;
        for (int i = 0; i < BL; i++) {
            Sample s = *input_samps++;
            sum += s * s;
        }
        return sum * BL_RECIP;
    }

    Sample dnsample_rms(Dnsampleb_state *state) {
        return sqrt(dnsample_power(state));
    }

    Sample dnsample_lowpass(Dnsampleb_state *state) {
        Sample sum = 0;
        for (int i = 0; i < BL; i++) {
            state->prev = alpha * *input_samps++ +
                          one_minus_alpha * state->prev;
        }
        return state->prev;
    }


    void real_run() {
        Sample_ptr input_samps = input->run(current_block); // update input
        Dnsampleb_state *state = &states[0];
        for (int i = 0; i < chans; i++) {
            *out_samps++ = (this->*dnsample)(state++);
            input_samps += input_stride;
        }
    }
};
