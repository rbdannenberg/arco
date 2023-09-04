// yin.h -- yin unit generator for pitch estimation
//
// Roger B. Dannenberg
// Aug 2023

/* yin is an audio unit generator that sends messages with
   pitch estimates.
*/

extern const char *Yin_name;

// Estimate a local minimum (or maximum) using parabolic
// interpolation. The parabola is defined by the points
// (x1,y1),(x2,y2), and (x3,y3).
float parabolic_interp(float x1, float x2, float x3,
                       float y1, float y2, float y3, float *min)
{
    float a, b, c;
    float pos;

    //  y1=a*x1^2+b*x1+c
    //  y2=a*x2^2+b*x2+c
    //  y3=a*x3^2+b*x3+c

    //  y1-y2=a*(x1^2-x2^2)+b*(x1-x2)
    //  y2-y3=a*(x2^2-x3^2)+b*(x2-x3)

    //  (y1-y2)/(x1-x2)=a*(x1+x2)+b
    //  (y2-y3)/(x2-x3)=a*(x2+x3)+b

    a = ((y1 - y2) / (x1 - x2) - (y2 - y3) / (x2 - x3)) / (x1 - x3);
    b = (y1 - y2) / (x1 - x2) - a * (x1 + x2);
    c = y1 -a * x1 * x1 - b * x1;

    // dy/dx = 2a*x + b = 0
  
    pos = -b / (a + a);
    *min = /* ax^2 + bx + c */ (a * pos + b) * pos + c;
    return pos;
}


class Yin : public Windowed_input {
  public:
    struct Yin_state {
        float harmonicity;
        float pitch;
        float rms;
    };
    Vec<Yin_state> yin_states;
    bool new_estimates; // set to true when yin runs
    const char *address; // where to send messages

    int m;  // shortest period in samples
    int middle;  // middle index of window
    float *results;  // temporary storage for yin
    
    Yin(int id, int chans, Ugen_ptr inp, int minstep, int maxstep,
        int hopsize, const char *address_) : Windowed_input(id, chans, inp) {
        yin_states.init(chans);
        middle = std::ceil(AR / step_to_hz(minstep));
        int window_size = middle * 2;
        Windowed_input::init(window_size + BL * 2, window_size, hopsize);
        m = AR / step_to_hz(maxstep);
        results = O2_MALLOCNT(middle - m + 1, float);
        init_inp(inp);
        new_estimates = false;
        address = o2_heapify(address_);
    }


    ~Yin() {
        O2_FREE(results);
        yin_states.finish();
        O2_FREE((char *) address);
    }

    const char *classname() { return Yin_name; }

    void repl_inp(Ugen_ptr inp_) {
        inp->unref();
        init_inp(inp_);
    }


    void init_inp(Ugen_ptr ugen) {
        init_param(ugen, inp, inp_stride);
        // TODO: we should fail gracefully in this case:
        assert(inp->rate == 'a');
    }


    void process_window(int channel, Sample_ptr window) {
        float left, right;  // samples from left period and right period
        float left_energy = 0;
        float right_energy = 0;
        float auto_corr;
        float cum_sum = 0.000001;  // avoid divide-by-zero on zero input
        float period;
        int min_i;
        const float threshold = 0.1F;
        
        // for each window, we keep the energy so we can compute the next one 
        // incrementally. First, we need to compute the energies for lag m-1:
        for (int i = 0; i < m - 1; i++) {
            left = window[middle - 1 - i];
            left_energy += left * left;
            right = window[middle + i];
            right_energy += right * right;
        }
        
        for (int i = m; i <= middle; i++) {
            // i is the lag and the length of the window
            // compute the energy for left and right
            left = window[middle - i];
            left_energy += left * left;
            right = window[middle - 1 + i];
            right_energy += right * right;
            //  compute the autocorrelation
            auto_corr = 0;
            for (int j = 0; j < i; j++) {
                auto_corr += window[middle - i + j] * window[middle + j];
            }
            float non_periodic = (left_energy + right_energy - 2 * auto_corr);
            results[i - m] = non_periodic;
        }
        
        // normalize by the cumulative sum
        for (int i = m; i <= middle; i++) {
            cum_sum += results[i - m];
            results[i - m] = results[i - m] / ( cum_sum / (i - m + 1));
        }

        min_i = m;  // value of initial estimate
        for (int i = m; i <= middle; i++) {
            if (results[i - m] < threshold) {
                min_i = i;
                // This step is not part of the published
                // algorithm. Just because we found a point below the
                // threshold does not mean we are at a local
                // minimum. E.g. a sine input will go way below
                // threshold, so the period estimate at the threshold
                // crossing will be too low. In this step, we continue
                // to scan forward until we reach a local minimum.
                while (min_i < middle &&
                       results[min_i + 1 - m] < results[min_i - m]) {
                    min_i++;
                }
                break;
            }
            if (results[i - m] < results[min_i - m]) {
                min_i = i;
            }
        }
        // use parabolic interpolation to improve estimate
        if (min_i > m && min_i < middle) {
            period = parabolic_interp(
                    (float)(min_i - 1), (float)(min_i), (float)(min_i + 1), 
                    results[min_i - 1 - m], results[min_i - m], 
                    results[min_i + 1 - m],
                    &(yin_states[channel].harmonicity));
        } else {
            period = (float) min_i;
            yin_states[channel].harmonicity = results[min_i - m];
        }
        yin_states[channel].pitch = (float) hz_to_step((float) (AR / period));
        // rms = sqrt(sum_of_squares / n)
        yin_states[channel].rms = sqrt((right_energy + left_energy) /
                                       (2 * middle));
        new_estimates = true;
    }


    void real_run() {
        Windowed_input::real_run();
        if (new_estimates) {  // time to send a message
            // message format is pitch0, harmo0, pitch1, harmo1, ...
            o2sm_send_start();
            for (int channel = 0; channel < chans; channel++) {
                Yin_state *ys = &yin_states[channel];
                o2sm_add_float(ys->pitch); 
                o2sm_add_float(ys->harmonicity);
                o2sm_add_float(ys->rms);
            }
            o2sm_send_finish(0, address, false);
            new_estimates = false;
        }
    }
};
