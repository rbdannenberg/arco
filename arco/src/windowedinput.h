// windowedinput.h -- superclass for processing (overlapping) windows
//
// Roger B. Dannenberg
// Aug 2023

// This is an abstract class for processing overlapping windows. When a
// window becomes available, process_window() is called, so subclasses
// should override this.
//
// What's a good buffer size? Algorithm is to shift samples to zero
// when we run off the end of the buffer (we do not use a circular
// buffer because we want to give the subclass a contiguous window.
// If hopsize is small, we could spend a lot of time copying, but
// copying should be less expensive than actually using the window,
// so maybe it doesn't really matter much. Here, we allocate the
// buffer size specified by the subclass in init(), but suggest that
// the window size is fine, but add a couple of BL to make sure we
// can use the window before we run out of room to add BL samples
// at each call to real_run(). Also, we increase the buffer to be
// at least the window size + 2 * BL, and then increase to the number
// of blocks actually allocated (since the memory allocator allocates
// in standard sizes and is likely to round up).

class Windowed_input : public Ugen {
  public:
    struct Windowed_input_state {
        Vec<float> samps;
    };
    Vec<Windowed_input_state> states;
    bool states_initialized;
    int tail;  // index of start of next window
    int window_size;  // size of window
    int hopsize;
    
    Ugen_ptr inp;
    int inp_stride;
    Sample_ptr inp_samps;

    Windowed_input(int id, int chans, Ugen_ptr inp) : Ugen(id, 'a', chans) {
        states.init(chans);
        states.set_size(chans);  // just to make size correct
        states_initialized = false;
        init_inp(inp);
        tail = 0;
    }

    virtual void process_window(int i, Sample_ptr window) = 0;
    
    void init(int buffer_size, int window_size_, int hopsize_) {
        buffer_size = MAX(buffer_size, window_size + BL * 2);
        for (int i = 0; i < chans; i++) {
            states[i].samps.init(buffer_size);
        }
        window_size = window_size_;
        hopsize = hopsize_;
        states_initialized = true;
    }
    

    ~Windowed_input() {
        assert(states_initialized);
        for (int i = 0; i < chans; i++) {
            states[i].samps.finish();
        }
        // states is destroyed automatically because it is a member variable
    }


    void repl_inp(Ugen_ptr inp_) {
        inp->unref();
        init_inp(inp_);
    }


    void init_inp(Ugen_ptr ugen) {
        init_param(ugen, inp, inp_stride);
        // TODO: we should fail gracefully in this case:
        assert(inp->rate == 'a');
    }


    void real_run() {
        assert(states_initialized);
        inp_samps = inp->run(current_block);
        Windowed_input_state *state = &states[0];
        if (state->samps.size() + BL > state->samps.get_allocated()) {
            int erase = MIN(tail, state->samps.size());
            // shift samples so that tail is 0
            for (int i = 0; i < chans; i++) {
                state->samps.erase(0, erase);
                state++;
            }
            tail -= erase;
        }
        
        state = &states[0];
        assert(state->samps.size() + BL <= state->samps.get_allocated());
        for (int i = 0; i < chans; i++) {
            state->samps.append(inp_samps, BL);  // append samples to window
            inp_samps += inp_stride;
            state++;
        }

        while ((state = &states[0])->samps.size() >= tail + window_size) {
            for (int i = 0; i < chans; i++) {
                process_window(i, &(state->samps[tail]));
                state++;
            }
            tail = tail + hopsize;
        }
    }
};
