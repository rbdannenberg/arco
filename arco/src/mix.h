/* mix.h -- unit generator that passes audio through
 *
 * Roger B. Dannenberg
 * Feb 2022
 */

extern const char *Mix_name;

class Mix : public Ugen {
public:
    struct Input {
        const char *name;
        Ugen_ptr input;
        int input_stride;
        Ugen_ptr gain;
        int gain_stride;
        Vec<float> prev_gain;

        // fade-in/fade-out state - see fader for more comments
        // the implementation of fade here is taken from the
        // fader implementation, and most of these state variables
        // correspond to fader instance variables:
        int fade_mode;
        float fade_goal;
        float fade_delta;
        float fade_factor;
        float fade_base;
        float fade_phase;
        float fade_count;
        float fade_dest;
        bool ar_smooth;  // there is a special case for short
                // FADE_SMOOTH fades where we compute the
                // envelope at the audio rate
    };
    Vec<Input> inputs;
    bool wrap;

    Mix(int id, int nchans, int wrap_) : Ugen(id, 'a', nchans) {
        tail_blocks = 1;  // because gain inputs are b-rate
        wrap = (wrap_ != 0);
    };

    ~Mix();

    const char *classname() { return Mix_name; }

    void print_sources(int indent, bool print_flag);

    // insert operation takes a signal and a gain
    void ins(const char *name, Ugen_ptr input, Ugen_ptr gain,
             float dur, int mode) {
        if (input->rate != 'a') {
            arco_warn("mix_ins: input rate is not 'a', ignore insert");
            return;
        }
        if (gain->rate == 'a') {
            arco_warn("mix_ins: gain rate must be 'b' or 'c', ignore insert");
            return;
        }
        if (gain->chans != 1 && input->chans != 1 &&
            gain->chans != input->chans) {
            arco_warn("mix_ins: input has %d chans, gain has %d chans",
                      input->chans, gain->chans);
            return;
        }
        int i = find(name, false);
        Input *input_desc;
        if (i >= 0) {  // name already exists; replace input
            input_desc = &inputs[i];
            input_desc->input->unref();
            input_desc->gain->unref();
        } else {  // allocate space for new input and initialize
            input_desc = inputs.append_space(1);
            input_desc->prev_gain.init(0);
        }

        input_desc->name = o2_heapify(name);
        int ignore_stride;  // inputs are always audio and stride is always 1
        init_param(input, input_desc->input, &input_desc->input_stride);
        init_param(gain, input_desc->gain, &input_desc->gain_stride);
        // initialize Vec of per-channel prev_gain value(s) and zero them:
        input_desc->prev_gain.set_size(MAX(input_desc->input->chans,
                                           input_desc->gain->chans), true);
        input_desc->fade_dest = 1.0;
        input_desc->fade_count = MAX(1, (int) (dur * BR + 0.5));
        input_desc->ar_smooth = false;
        input_desc->fade_mode = mode;
        if (input_desc->fade_count > 0) {
            switch (mode) {
                case FADE_LINEAR:
                    input_desc->fade_phase = 0;  // represents current fade val
                    input_desc->fade_delta = 1.0f / input_desc->fade_count;
                    break;
                case FADE_EXPONENTIAL:
                    input_desc->fade_phase = 0;  // represents current fade val
                    input_desc->fade_factor =
                            pow(101.0, 1.0 / input_desc->fade_count);
                    break;
                case FADE_LOWPASS:
                    input_desc->fade_base = 1.01;
                    input_desc->fade_delta = -1.01;
                    input_desc->fade_factor =
                            pow(0.01 / 1.01, 1.0 / input_desc->fade_count);
                    break;
                case FADE_SMOOTH:
                default: {
                    input_desc->fade_delta = float(-COS_TABLE_SIZE) /
                                                   input_desc->fade_count;
                    input_desc->fade_factor = 1.0;
                    input_desc->fade_base = 1.0;
                    input_desc->fade_phase = 2 + COS_TABLE_SIZE;
                    if (input_desc->fade_count < 0.01 * BR) {
                        // shorter than 10msec, needs special case
                        input_desc->ar_smooth = true;
                        input_desc->fade_delta *= BL_RECIP;
                    }
                    break;
                }
            }
        }
    }


    // find the index of Ugen input -- linear search
    int find(const char *name, bool expected = true) {
        for (int i = 0; i < inputs.size(); i++) {
            Input *input_desc = &inputs[i];
            if (strcmp(input_desc->name, name) == 0) {
                return i;
            }
        }
        if (expected) {
            arco_warn("Mix::find: %s not found, nothing was changed", name);
        }
        return -1;
    }
        

    // remove operation finds the signal and removes it and its gain
    void rem(char *name, float dur, int mode) {
        int i = find(name);
        if (i >= 0) {
            Input &input_desc = inputs[i];
            input_desc.fade_dest = 0.0;
            input_desc.fade_count = MAX(1, (int) (dur * BR + 0.5));
            if (input_desc.fade_count > 0) {
                switch (mode) {
                    case FADE_LINEAR:
                        input_desc.fade_phase = 1;  // represents current fade val
                        input_desc.fade_delta = -1.0f / input_desc.fade_count;
                        break;
                    case FADE_EXPONENTIAL:
                        input_desc.fade_phase = 1;  // represents current fade val
                        input_desc.fade_factor =
                                pow(1.0 / 101.0, 1.0 / input_desc.fade_count);
                        break;
                    case FADE_LOWPASS:
                        input_desc.fade_base = -0.01;
                        input_desc.fade_delta = 1.01;
                        input_desc.fade_factor =
                                pow(0.01 / 1.01, 1.0 / input_desc.fade_count);
                        break;
                    case FADE_SMOOTH:
                    default: {
                        input_desc.fade_delta = float(-COS_TABLE_SIZE) /
                                                      input_desc.fade_count;
                        input_desc.fade_factor = -1.0;
                        input_desc.fade_base = 0.0;
                        input_desc.fade_phase = 2 + COS_TABLE_SIZE;
                        if (input_desc.fade_count < 0.01 * BR) {
                            // shorter than 10msec, needs special case
                            input_desc.ar_smooth = true;
                            input_desc.fade_delta *= BL_RECIP;
                        }
                        break;
                    }
                }
            } else {  // no fade-out, no wait:
                remove_input(i, &inputs[i]);
            }
        }
    }


    void remove_input(int i, Input *input_desc) {
        O2_FREE((void *) input_desc->name);
        input_desc->name = NULL;
        input_desc->input->unref();
        input_desc->input = NULL;
        input_desc->gain->unref();
        input_desc->gain = NULL;
        inputs.remove(i);
    }


    void set_gain(const char *name, int chan, float gain) {
        int i = find(name);
        if (i >= 0) {
            Input *input_desc = &inputs[i];
            Ugen *gain_ugen = input_desc->gain;
            if (gain_ugen->rate == 'c') {
                gain_ugen->const_set(chan, gain, "Mix::set_gain");
            } else if (chan == 0) {
                gain_ugen->unref();
                init_param(new Const(-1, 1, gain), input_desc->gain,
                           &input_desc->gain_stride);
                input_desc->prev_gain.set_size(input_desc->input->chans, true);
                // zero prev_gain if number of channels changes:
                if (input_desc->input->chans != input_desc->prev_gain.size()) {
                    input_desc->prev_gain.zero();
                }
            } else {
                printf("WARNING: In Mix::set_gain, current gain is not Const"
                       " and chan to set is not zero, mix %d, name %s,"
                       " chan %d\n", id, name, chan);
            }
        }
    }


    void repl_gain(char *name, Ugen_ptr gain) {
        int i = find(name);
        if (i >= 0) {
            Input *input_desc = &inputs[i];
            if (gain->chans != 1 && input_desc->input->chans != 1 &&
                gain->chans != input_desc->input->chans) {
                    arco_warn("mix_repl_gain: %s input has %d chans, new gain "
                              "has %d chans", name, input_desc->input->chans,
                              gain->chans);
                    return;
            }
            input_desc->gain->unref();
            init_param(gain, input_desc->gain, &input_desc->gain_stride);
            int new_chans = MAX(input_desc->input->chans, gain->chans);
            input_desc->prev_gain.set_size(new_chans, true);
            // zero prev_gain if number of channels changes:
            if (input_desc->input->chans != input_desc->prev_gain.size()) {
                input_desc->prev_gain.zero();
            }
        }
    }


    void real_run() {
        int starting_size = inputs.size();
        int i = 0;

        // zero the outputs
        block_zero_n(out_samps, chans);

        while (i < inputs.size()) {  // use while loop because size can change
            Input &input_desc = inputs[i];
            float *gain_ptr = input_desc.gain->run(current_block);
            float *gprev_ptr = &(input_desc.prev_gain[0]);
            Sample_ptr input_ptr = input_desc.input->run(current_block);
            float *out = out_samps;

            if ((input_desc.input->flags & TERMINATED) ||
                (input_desc.gain->flags & TERMINATED)) {
                remove_input(i, &input_desc);
                continue;
            }
            assert(input_desc.prev_gain.size() ==
                   MAX(input_desc.input->chans, input_desc.gain->chans));

            // compute the next fade value (or fade_dest if not fading)
            float fade = input_desc.fade_dest;
            if (input_desc.fade_count > 0) {
                switch (input_desc.fade_mode) {
                    case FADE_LINEAR:
                        input_desc.fade_phase += input_desc.fade_delta;
                        fade = input_desc.fade_phase;
                        break;
                    case FADE_EXPONENTIAL:
                        fade = input_desc.fade_phase;
                        fade = (fade + 0.01) * input_desc.fade_factor - 0.01;
                        input_desc.fade_phase = fade;
                        break;
                    case FADE_LOWPASS:
                        input_desc.fade_delta *= input_desc.fade_factor;
                        fade = input_desc.fade_base + input_desc.fade_delta;
                        break;
                    case FADE_SMOOTH:
                    default: {
                      if (!input_desc.ar_smooth) {
                          input_desc.fade_phase += input_desc.fade_delta;
                          int phasei = (int) input_desc.fade_phase;
                          fade = raised_cosine[phasei];
                          // linear interpolation
                          fade += (raised_cosine[phasei + 1] - fade) *
                                  (input_desc.fade_phase - phasei);
                          fade = input_desc.fade_base -
                                 input_desc.fade_factor * fade;
                      }  // otherwise, ar_smooth case is handled below
                      break;
                    }
                }
                input_desc.fade_count--;
            } else if (fade == 0) {  // fade-out completed
                remove_input(i, &input_desc);
                continue;
            } else if (input_desc.ar_smooth) {  // convert from audio rate
                // to block rate after fade-in completes.
                // *gprev_ptr is previous gain before multiplying by fade
                // but in the normal block-rate processing, *gprev_ptr
                // includes the factor of fade:
                *gprev_ptr *= fade;
                input_desc.ar_smooth = false;
            }

            for (int ch = 0; ch < input_desc.prev_gain.size(); ch++) {
                float gain = *gprev_ptr;
                if (input_desc.ar_smooth) {  // compute fade at audio rate
                    float gincr = (*gain_ptr - gain) * BL_RECIP;
                    float phase = input_desc.fade_phase;
                    for (int j = 0; j < BL; j++) {
                        gain += gincr;
                        phase += input_desc.fade_delta;
                        int phasei = (int) phase;
                        fade = raised_cosine[phasei];
                        fade += (raised_cosine[phasei + 1] - fade) *
                                (phase - phasei);
                        fade = input_desc.fade_base -
                               input_desc.fade_factor * fade;
                        *out++ += gain * fade * input_ptr[j];
                    }
                    *gprev_ptr = gain;
                } else {
                    float gincr = (*gain_ptr * fade - gain) * BL_RECIP;
                    for (int j = 0; j < BL; j++) {
                        gain += gincr;
                        *out++ += gain * input_ptr[j];
                    }
                    *gprev_ptr = gain;
                }

                if (out >= out_samps + BL * chans) {  // wrap to output
                    if (wrap) {
                        out = out_samps;
                    } else {
                        break;
                    }
                }
                // advance to the next channel of this input:
                input_ptr += input_desc.input_stride;
                gain_ptr += input_desc.gain_stride;
                gprev_ptr++;  // we have a prev_gain for each generated channel
                        // even if gain is single channel (gain_stride == 0)
            }
            i++;
        }
        if (inputs.size() == 0 && starting_size > 0 &&
            (flags & CAN_TERMINATE)) {
            terminate();
        }
    }
};


    
