// yin.h -- yin for software oscilloscope
//
// Roger B. Dannenberg
// Aug 2023

/* yin is an audio unit generator that sends messages with
   pitch estimates.
*/

extern const char *Yin_name;

class Yin : public Windowed_input {
  public:
    struct Yin_state {
        float harmonicity;
        float pitch;
    };
    Vec<Yin_state> yin_states;
    bool new_estimates; // set to true when yin runs
    char *address; // where to send messages

    int m;  // shortest period in samples
    int middle;  // middle index of window
    float *results;  // temporary storage for yin
    
    Yin(int id, int chans, Ugen_ptr inp, int minstep, int maxstep,
                int hopsize, char *address) : Windowed_input(id, chans, inp) {
        yin_states.init(chans);
        middle = std::ceil(AR / step_to_hz(minstep));
        int window_size = middle * 2;
        windowed_input_init(window_size + BL * 2, window_size, hopsize);
        m = AR / step_to_hz(high_step);
        results = O2_MALLOC(sizeof(float) * (middle - m + 1));
        init_inp(inp);
        new_estimates = false;
        address = o2_heapify(address);
    }


    ~Yin() {
        O2_FREE(results);
        yin_states.finish();
        O2_FREE(address);
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


    void process_window(int i, float window) {
        float left, right;  // samples from left period and right period
        float left_energy = 0;
        float right_energy = 0;
        float auto_corr;
        float cum_sum = 0;
        float period;
        int min_i;
        float threshold = 0.1F;
        
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
        for (i = m; i <= middle; i++) {
            cum_sum += results[i - m];
            results[i - m] = results[i - m] / (cum_sum / (i - m + 1));
        }

        min_i = m;  // value of initial estimate
        for (i = m; i <= middle; i++) {
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
                    results[min_i + 1 - m], &(yin_states[i].harmonicity));
        } else {
            period = (float) min_i;
            yin_states[i].harmonicity = results[min_i - m];
        }
        yin_states[i].pitch = (float) hz_to_step((float) (AR / period));
        new_estimates = true;
    }


    void prepare_msg() {
        msg = (O2message_ptr) O2_MALLOC(msg_len);
        memcpy(msg, msg_header, msg_header_len);
        sample_ptr = (float *) (((char *) msg) + msg_header_len);
        sample_fence = (float *) (((char *) msg) + msg_len);
    }


    void real_run() {
        Windowed_input::real_run();
        if (new_estimates) {  // time to send a message
            // message format is pitch0, harmo0, pitch1, harmo1, ...
            o2_send_start();
            for (int i = 0; i < chans; i++) {
                o2_add_float(yin_states[i].pitch);
                o2_add_float(yin_states[i].harmonicity);
            }
            o2_send_finish(0, address, false);
            new_estimates = false;
        }
    }
};
