// trig.h -- trig unit generator for sound event detection
//
// Roger B. Dannenberg
// Aug 2023

/* trig is an audio unit generator that sends messages when
   RMS exceeds a threshold or when RMS indicates sound on
   and sound off.
*/

extern const char *Trig_name;


class Trig : public Ugen {
  public:

    const char *address; // where to send messages
    int window_size;  // in samples
    float trig_threshold;
    int pause;  // in blocks
    float sum0;  // sum for current window
    float sum1;  // sum for next window
    int count;   // how many samples processed since last window end
    bool enabled;  // true if enabled to detect audio event

    const char *onoff_addr;  // where to send onoff messages
    float onoff_threshold;   // threshold for onoff detection
    int onoff_runlen;        // measured in blocks
    bool onoff_state;        // current onoff detected
    int onoff_count;        // how many times state is repeated
    bool reported_state;     // last onoff reported
    int pause_for;           // number of blocks remaining in current pause

    Ugen_ptr input;
    int input_stride;
    Sample_ptr input_samps;

    
    Trig(int id, Ugen_ptr input, const char *address_, int window_size,
         float threshold, float pause) : Ugen(id, 0, 0) {
        // round up to multiple of BL:
        address = o2_heapify(address_);
        set_window(window_size);
        trig_threshold = threshold;
        set_pause(pause);
        init_input(input);
        // initial RMS will exceed any reasonable threshold, so the
        // first test, based on only 1/2 window size, will fail to set
        // enabled. We will become enabled as soon as a full window
        // RMS is below threshold:
        sum0 = 1.0e10;
        sum1 = 0;
        count = 0;
        onoff_addr = NULL;
        onoff_threshold = 0;
        onoff_count = 0;
        onoff_runlen = 2;  // this value is not used - see onoff() method
        reported_state = false;
        enabled = false;
    }


    ~Trig() {
        O2_FREE((char *) address);
        if (onoff_addr) {
            O2_FREE((char *) onoff_addr);
        }
    }


    void print_details(int indent) {
        arco_print("trig_threshold %g pause %d enabled %s",
                   trig_threshold, pause, enabled ? "true" : "false");
    }

   
    void print_sources(int indent, bool print_flag) {
        input->print_tree(indent, print_flag, "input");
    }


    void init_input(Ugen_ptr ugen) { init_param(ugen, input, input_stride); }

    const char *classname() { return Trig_name; }

    void onoff(const char *repl_addr, float threshold, float runlen) {
        if (onoff_addr) {
            O2_FREE((char *) onoff_addr);
        }
        if (!repl_addr[0]) {  // empty string means disable
            onoff_addr = NULL;
        } else {
            onoff_addr = o2_heapify(repl_addr);
            onoff_threshold = threshold;
            onoff_runlen = std::ceil(runlen * BR);
        }
    }


    void repl_input(Ugen_ptr ugen) {
        input->unref();
        init_input(ugen);
    }


    void set_window(int size) {
        window_size = (size + BL - 1) & ~ (BL - 1);
    }


    void set_threshold(float thresh) {
        trig_threshold = thresh;
    }


    void set_pause(float pause_) {
        pause = std::ceil(pause_ * BR);
    }
    
    
    void real_run() {
        input_samps = input->run(current_block);
        // compute sum of squares of input samples
        int input_chans = input->chans;
        float sum = 0;
        int n = input_chans * BL;
        for (int i = 0; i < n ; i++) {
            float s = *input_samps;
            sum += s * s;
        }
        sum0 += sum;
        sum1 += sum;
        count += BL;
        // end of half window?
        if (count >= (window_size >> 1)) {
            float rms = sqrtf(sum0 / (window_size * input->chans));
            if (enabled && rms > trig_threshold && pause_for <= 0) {
                o2sm_send_start();
                o2sm_add_int32(id);
                o2sm_add_float(rms);
                o2sm_send_finish(0, address, false);
                pause_for = pause;
                // set sum1 to sum0, which is above threshold, so that when
                // analysis resumes, the initial rms, which will be based on
                // only 1/2 window of new input, will also exceed threshold
                // so that the trigger will not be reenabled. The following
                // rms, based on a whole window, will be the first opportunity
                // to set enabled, and possibly on the third half-window (at
                // the earliest), we can generate another trigger.
                sum1 = sum0;
                enabled = false;
            } else if (sum0 < trig_threshold) {
                enabled = true;
            }

            if (onoff_addr) {  // is onoff mode enabled?
                if (rms > onoff_threshold) {
                    onoff_state = true;
                } else if (rms < onoff_threshold * 0.9) { // hysteresis
                    onoff_state = false;
                }
                onoff_count++;
                if (onoff_state == reported_state) {
                    onoff_count = 0;  // reset run length counter
                } else if (onoff_count >= onoff_runlen) {
                    reported_state = onoff_state;
                    o2sm_send_start();
                    o2sm_add_int32(id);
                    o2sm_add_int32((int) onoff_state);
                    o2sm_send_finish(0, onoff_addr, false);
                }
            }
            count = 0;
            sum0 = sum1;  // now sum1 (next window) is moved to sum0
            sum1 = 0;  // and sum1 (next window) is just starting, set to 0
        }
        pause_for--;
    }
};
