// pv.h -- Phase vocoder time stretch and pitch shift
//
// Roger B. Dannenberg
// July 2023
//
// see doc/design.md: "Phase Vocoder and Pitch Shifting"

extern const char *Fileplay_name;

extern const char *Pv_name;

class Pv: public Ugen {
  public:
    int points;
    int fftsize;
    int hopsize;
    float ratio;
    int mode;
    float stretch;  // time stretch factor
    bool use_stretch;  // use stretch to control when to read input
    // (use_stretch is true iff input is a Fileplay Ugen).

    struct Pv_state {
        // inputbuf is used to hold input samples -- they accumulate
        // here so that we can get fftsize samples each time the PV
        // needs more input. This is a sliding window sort of buffer:
        // when it fills to 2 * fftsize, we copy the last fftsize
        // samples to the beginning and start appending to it again:
        Vec<float> inputbuf;
        int prev_input_end;  // end index of last input to phase vocoder
        // outputbuf is needed because we ask for blocks of hopsize
        // samples, but we have to deliver them BL samples at a time.
        // This is also a sliding window and we retain 2 * points
        // samples for use by the resampling filter:
        Vec<float> outputbuf;
        double outputbuf_offset;  // where to take next (interpolated) sample
        bool outputbuf_filled;    // tells us to prime the pump on first time
        Phase_vocoder pv;
    };
    Vec<Pv_state> states;

    Ugen_ptr input;
    int input_stride;
    Sample_ptr input_samps;
    Resamp resamp;  // resampler for pitch shift -- shared by all states


    Pv(int id, int nchans, Ugen_ptr input, float ratio, int fftsize_,
       int hopsize_, int points_, int mode_) : Ugen(id, 'a', nchans) {
        init_input(input);
        points = points_;
        fftsize = fftsize_;
        hopsize = hopsize_;
        mode = mode_;
        states.set_size(chans);
        for (int i = 0; i < chans; i++) {
            states[i].inputbuf.init(fftsize * 2);
            // input is initially big enough for FFT if needed
            states[i].inputbuf.set_size(fftsize);  // zero fills
            // output
            states[i].outputbuf.init(hopsize + 3 * points);
            // initially, set up as if our offset for the next sample
            // is hopsize + points, but we only have hopsize + points in
            // the buffer, so we will immediately compute hopsize new
            // samples, which will slide the offset down to points and
            // insert the new samples from points to points + hopsize.
            // outputbuf can hold some extra samples to allow points to
            // be scaled when downsampling where we need a wider filter
            // for antialiasing.
            states[i].outputbuf.set_size(hopsize + points);  // zero fills
            states[i].outputbuf_offset = hopsize + points;
            states[i].outputbuf_filled = false;
            Phase_vocoder pv = pv_create(o2_malloc_ptr, o2_free_ptr);
            pv_set_fftsize(pv, fftsize);
            // this means we will get one analysis input window each time
            // we request output:
            pv_set_blocksize(pv, hopsize);
            pv_set_syn_hopsize(pv, hopsize);
            pv_set_mode(pv, mode);
            pv_initialize(pv);
            states[i].pv = pv;
        }
        resamp.init(points, 32);
        set_ratio(ratio);
    }

    ~Pv() {
        input->unref();
        for (int i = 0; i < chans; i++) {
            states[i].inputbuf.finish();
            states[i].outputbuf.finish();
            pv_end(&(states[i].pv));
        }
        resamp.finish();
    }


    const char *classname() { return Pv_name; }
    

    void print_details(int indent) {
        arco_print("ratio %g fftsize %d hopsize %d points %d mode %d",
                   ratio, fftsize, hopsize, points, mode);
    }


    void print_sources(int indent, bool print_flag) {
        input->print_tree(indent, print_flag, "input");
    }


    void init_input(Ugen_ptr ugen) {
        init_param(ugen, input, input_stride);
        use_stretch = (input->classname() == Fileplay_name);
    }


    void set_ratio(float r) {
        int max_hopsize = fftsize / 3;
        // theoretically, min_ratio should be hopsize / max_hopsize,
        // but in real-time pitch shift mode, input hopsizes are
        // rounded up to take us to the end of the buffer, and buffers
        // are incremented by the block size (BL), so sometimes due to
        // quantization, the hopsize is greater than what ratio would
        // imply. We divide by (max_hopsize - BL) because (I think)
        // this will cover the worst case and make the nominal input
        // hopsize (max_hopsize - BL). If quantization increases the
        // nominal hopsize by BL, then it becomes max_hopsize, which
        // is by definition tolerable. If the worst case happens and
        // we request an even larger hopsize, CMUPV limits the
        // hopsize, and an assert fails. Without the assert (non-debug
        // mode), *maybe* we just don't consume all the input yet, and
        // *maybe* we'll catch up later.
        float min_ratio = float(hopsize) / (max_hopsize - BL);
        if (r < min_ratio) {
            arco_warn("pv ugen set_ratio(%g): ratio too low. Using %g. "
                      "Use smaller hopsize for smaller ratios.\n",
                      r, min_ratio);
            r = min_ratio;
        }
        ratio = r;
    }


    void set_stretch(float s) {
        stretch = s;
    }


    void repl_input(Ugen_ptr ugen) {
        input->unref();
        init_input(ugen);
    }


    // get need samples from input, which must be a Fileplay Ugen
    void get_samples(int need) {
        while (need > 0) {  // get BL samples per iteration
            input_samps = input->run(input->current_block + BL);
            // move input_samps to inputbuf's
            for (int i = 0; i < chans; i++) {
                states[i].inputbuf.append(input_samps, BL);
                input_samps += input_stride;
            }
            need -= BL;
        }
    }


    void chan_a(Pv_state *state) {
        // fileplay
        Vec<Sample> &inputbuf = state->inputbuf;
        Vec<Sample> &outputbuf = state->outputbuf;
        // make sure we have room for input
        if (inputbuf.size() + BL > fftsize * 2) {
            // shift inputbuf to make room for more samples
            assert(inputbuf.size() >= fftsize);
            inputbuf.drop_front(fftsize);
            state->prev_input_end -= fftsize;
        }
        // append input_samps to inpututbuf if input is synchronous
        // (not stretched). E.g. we have !use_stretch when input is
        // real-time audio (not stored)
        if (!use_stretch) {
            assert(inputbuf.get_allocated() >= inputbuf.size() + BL);
            inputbuf.append(input_samps, BL);
        }  // otherwise, we fetch input on demand
        int done = 0;
        while (done < BL) {
            // how big is the resampling window?
            float scale = MAX(1.0, ratio);
            // winend is the offset of the right edge of the interpolation
            // window. We need samples through floor(winend) for interpolation:
            double winend = state->outputbuf_offset + scale * (points / 2);
            Phase_vocoder pv = state->pv;
            if (winend > outputbuf.size() - 1 || !state->outputbuf_filled) {
                // Need more output samples. First, slide buffer:
                assert(outputbuf.size() >= hopsize);
                outputbuf.drop_front(hopsize);
                state->outputbuf_offset -= hopsize;
                winend -= hopsize;
                int need;
                if (use_stretch) {  // input count depends on stretch,
                    // which only works with Playfile where we can fetch at
                    // any rate:
                    pv_set_ratio(pv, stretch);
                    int need = pv_get_input_count(pv);
                    // Put need samples into inputbufs:
                    get_samples(need);
                    assert(inputbuf.size() >= need);
                    pv_put_input(pv, need, &inputbuf[0]);
                    inputbuf.drop_front(need);
                    state->prev_input_end = 0; // where to take next sample
                } else if (!state->outputbuf_filled) {  // first time
                    // Transfer needed samples to PV:
                    need = pv_get_input_count(pv);
                    // initially, cmupv is primed with 1/2 window of zeros
                    assert(need == fftsize / 2);
                    assert(inputbuf.size() > need);
                    pv_put_input(pv, need, &inputbuf[inputbuf.size() - need]);
                    state->outputbuf_filled = true;
                    // we took up to the last sample in inputbuf:
                    state->prev_input_end = inputbuf.size();
                } else {  // ignore stretch, compute ratio to consume as
                    // much input is available. On average, PV ratio will
                    // compensate for resampling ratio so that we consume
                    // input at the real-time sample rate while producing
                    // output at the real-time sample rate (AR). There is
                    // no attempt to smooth out the PV ratio, so the input
                    // hopsize will jump in multiples of BL (32) samples.
                    int available = inputbuf.size() - state->prev_input_end;
                    pv_set_ratio(pv, hopsize / float(available));
                    need = pv_get_input_count(pv);
                    assert(need == available);
                    assert(inputbuf.size() >= state->prev_input_end + need);
                    pv_put_input(pv, need, &inputbuf[state->prev_input_end]);
                    // we took up to the last sample in inputbuf:
                    state->prev_input_end = inputbuf.size();
                }
                
                // now get cmupv to fill the buffer
                assert(outputbuf.get_allocated() >= outputbuf.size() + hopsize);
                outputbuf.append(pv_get_output(pv), hopsize);
            }
            // produce result by resampling samples in outputbuf; count is
            // the number of result samples to produce. For one sample, we
            // need samples through floor(winend). How many times can we
            // advance by ratio before we go past the last sample? Note that
            // we add one because we can compute one sample without
            // incrementing winend.
            assert(outputbuf.size() - 1 >= winend);
            int count = (outputbuf.size() - 1 -  winend) / ratio + 1;
            if (count > BL - done) {
                count = BL - done;
            }
            assert(count > 0);
            assert(state->outputbuf_offset + (count - 1) * ratio +
                   0.5 * scale * resamp.span < outputbuf.size());
            resamp.resamp(out_samps, count, &outputbuf[0],
                          state->outputbuf_offset, scale, ratio);
            out_samps += count;
            done += count;
            state->outputbuf_offset += count * ratio;
        }
    }

    
    void real_run() {
        if (!use_stretch) {
            input_samps = input->run(current_block);
        }
        for (int i = 0; i < chans; i++) {
            chan_a(&states[i]);
            input_samps += input_stride;
        }
    }

};


