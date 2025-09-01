/* stdistr.h -- unit generator that distributes inputs across stereo field
 *
 * Roger B. Dannenberg
 * Oct 2024
 */

// Algorithm/design: Assume gain and width are not varied continuously,
// but we want changes to be smooth, so we apply rate limiting to both,
// with a minumum 50ms sweep time from 0 to 1. After either changes,
// we recompute gains, which are individual left/right gains for each
// input and increments to linearly interpolate up from block rate
// changes. We set increments to zero when there's no change in gain
// or width.

extern const char *Stdistr_name;

class Stdistr : public Ugen {
public:
    float gain;  // final output gain
    float prev_gain;
    float width; // stereo spread of inputs
    float prev_width;
    bool changing;
    bool zero_increments;

    Vec<Ugen_ptr> inputs;
    Vec<float> gains;  // interleaved left, left_incr, right, right_incr - gain state
                       // for each input

    Stdistr(int id, int n, float width) : Ugen(id, 'a', 2 /* chans */) {
        if (n < 2) {
            n = 2;
        }
        inputs.set_size(n);
        gains.set_size(4 * n);
        this->width = width;
        prev_width = width;
        gain = 1;
        prev_gain = 1; 
        changing = true;  // force gains computation on first real_run
    }

    ~Stdistr() {
        for (int i = 0; i < inputs.size(); i++) {
            if (inputs[i]) {
                inputs[i]->unref();
            }
        }
        // since inputs is a member, ~Vec will run now and delete it
    }

    const char *classname() { return Stdistr_name; }


    void print_sources(int indent, bool print_flag) {
        for (int i = 0; i < inputs.size(); i++) {
            if (inputs[i]) {
                char name[8];
                snprintf(name, 8, "%d", i);
                inputs[i]->print_tree(indent, print_flag, name);
            }
        }
    }


    void set_gain(float g) {
        gain = g;
        changing = true;
    }


    void set_width(float w) {
        width = w;
        changing = true;
    }


    // insert operation takes an index and signal
    void ins(int i, Ugen_ptr input) {
        assert(input->chans > 0);
        if (input->rate != 'a') {
            arco_warn("stdistr_ins: input rate is not 'a', ignore insert");
            return;
        }
        if (input->chans == 0) {
            arco_warn("stdistr_ins: input has zero output channels");
            return;
        }
        if (input->chans > 1) {
            arco_warn("stdistr_ins: input has %d channel but only the"
                      " first is used", input->chans);
        }
        if (i < inputs.size()) {  // input is not already in stdistr; append it
            if (inputs[i]) {
                inputs[i]->unref();
            }
            inputs[i] = input;
            input->ref();
        }
        /*
        printf("After insert, stdistr inputs (%p) has\n", inputs.get_array());
        for (i = 0; i < inputs.size(); i++) {
            printf("    %p: %s\n", inputs[i], inputs[i]->classname());
        }
        */
    }


    // remove operation finds the signal and removes it and its gain
    void rem(int i) {
        if (i >= 0 && i < inputs.size()) {
            inputs[i]->unref();
            inputs[i] = nullptr;
        }
    }


    void real_run() {
        const float SLEW_INCR = (BP / 0.050);  // max increment per block
        int n = inputs.size();
        // are gains changing? If so, update them:
        if (changing) {
            if (gain > prev_gain + SLEW_INCR) {
                prev_gain += SLEW_INCR;
            } else if (gain < -prev_gain - SLEW_INCR) {
                prev_gain -= SLEW_INCR;
            } else if (fabs(gain - prev_gain) < 1e-6) {
                prev_gain = gain;
            }

            if (width > prev_width + SLEW_INCR) {
                prev_width += SLEW_INCR;
            } else if (width < -prev_width - SLEW_INCR) {
                prev_width -= SLEW_INCR;
            } else if (fabs(width - prev_width) < 1e-6) {
                prev_width = width;
            }

            for (int i = 0; i < n; i++) {
                // compute gains and increments
                float pan = (float(i) / (n - 1)) * prev_width +
                            (0.5 - prev_width / 2);

                // pan 0 to 1 maps to
                //     COS_TABLE_SIZE + 2 to COS_TABLE_SIZE / 2 + 2
                float angle = (COS_TABLE_SIZE + 2) - 
                              (pan * (COS_TABLE_SIZE / 2.0));
                int anglei = (int) angle;
                float left = raised_cosine[anglei];
                left += (angle - anglei) * (left - raised_cosine[anglei + 1]);
                // now left is from 0.5 to 1 because we used raised_cosine,
                // but we want 0 to 1 as in cosine, so fix it
                left += left - 1;
                left *= prev_gain;
                gains[i * 4 + 1] = (left - gains[i * 4]) * BL_RECIP;

                // pan 0 to 1 maps to COS_TABLE_SIZE / 2 + 2 to COS_TABLE_SIZE + 2
                angle = (COS_TABLE_SIZE * 3 / 2.0f) - angle;
                anglei = (int) angle;
                float right = raised_cosine[anglei];
                right += (angle - anglei) * (right - raised_cosine[anglei + 1]);
                // now right is from 0.5 to 1 because we used raised_cosine,
                // but we want 0 to 1 as in cosine, so fix it
                right += right - 1;
                right *= prev_gain;
                gains[i * 4 + 3] = (right - gains[i * 4 + 2]) * BL_RECIP;

                changing = ((prev_gain != gain) || (prev_width != width));
                if (!changing) {  // next time, gains ramps will finish, so set
                    zero_increments = true;  // gains increments to zero
                }
            }
        } else if (zero_increments) {
            for (int i = 0; i < n; i++) {
                gains[i * 4 + 1] = 0;
                gains[i * 4 + 3] = 0;
            }
            zero_increments = false;
        }

        // now, gains are ready for processing:
        block_zero_n(out_samps, 2);
        float *pgains = &gains[0];  // faster (native C) array access
        for (int i = 0; i < n; i++) {
            Ugen_ptr input = inputs[i];
            if (!input) {
                continue;
            }
            Sample_ptr input_ptr = input->run(current_block);
            if (input->flags & TERMINATED) {
                input->unref();
                inputs.remove(i);
                continue;
            }

            float left_gain = pgains[i * 4];
            float right_gain = pgains[i * 4 + 2];
            for (int j = 0; j < BL; j++) {
                left_gain += pgains[i * 4 + 1];
                right_gain += pgains[i * 4 + 3];
                out_samps[j] += left_gain * input_ptr[j];
                out_samps[j + BL] += right_gain * input_ptr[j];
            }
            pgains[i * 4] = left_gain;
            pgains[i * 4 + 2] = right_gain;
        }
    }
};
