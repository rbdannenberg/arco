/* thru.h -- unit generator that passes audio through
 *
 * Roger B. Dannenberg
 * Jan 2022
 */

extern const char *Thru_name;

class Thru : public Ugen {
public:
    Ugen_ptr input;
    int input_stride;
    Ugen_ptr alternate;  // allow input to come from another Ugen

    Thru(int id, int nchans, Ugen_ptr input) : Ugen(id, 'a', nchans) {
        // printf("Thru@%p created, id %d, ugen_table[id] %p\n",
        //        this, id, ugen_table[id]);
        init_input(input);
        alternate = NULL;
    };

    ~Thru() { input->unref(); }

    const char *classname() { return Thru_name; }

    void print_sources(int indent, bool print_flag) {
        input->print_tree(indent, print_flag, "input");
    }

    void repl_input(Ugen_ptr ugen) {
        input->unref();
        init_input(ugen);
    }

    void set_alternate(Ugen_ptr alt) {
        assert(alt);
        if (alternate) {
            alternate->unref();
        }
        if (alt->id == ZERO_ID) {  // clear the alternate by setting to Zero
            alternate = NULL;
            return;
        }
        if (alt->chans != chans && alt->chans != 1) {
            arco_warn("Thru set_alternate channel mismatch, not set");
            return;
        }
        alternate = alt;
        alternate->ref();
    }


    void init_input(Ugen_ptr ugen) {
        assert(ugen->rate == 'a');
        init_param(ugen, input, &input_stride);
    }


    void real_run() {
        // If this is the Thru representing input, three interesting things
        // happen around real_run():
        // 1. The input is always a Zero, so input->run() just compares
        //    current_block to a very big number and returns.
        // 2. Normally, we never even call real_run() because the
        //    out_samps are written by the audio callback with incoming
        //    samples and current_block is set to aud_blocks_done, so
        //    real_run() is not called. Otherwise, the code inside
        //    if (!alternate) would overwrite the input.
        // 3. If the Thru redirects to an alternate input, this is a
        //    special case handled in the audio callback_entry() function
        //    and even though input is copied to out_samps, current_block
        //    is not updated to aud_blocks_done, so this is the only time
        //    the input Thru's real_run() is called. Since alternate is
        //    non-NULL, we do not copy from input (Zero) to output and
        //    instead call alternate->run() and copy samples from there
        //    to out_samps. There is some careful code to handle mismatch
        //    in channel counts.
        Sample_ptr src = input->run(current_block);  // bring input up-to-date
        if (!alternate) {  // redirect caller to get samples from alternate
            for (int i = 0; i < chans; i++) {
                block_copy(out_samps, src);
                src += input_stride;
                out_samps += BL;
            }
        } else {  // redirect caller to get samples from alternate
            Sample_ptr src = alternate->run(current_block);
            Sample_ptr src_end = src + BL * alternate->chans;
            int stride = (alternate->chans == 1 ? 0 : BL);
            int i;
            int chans_to_copy = MIN(chans, alternate->chans);
            for (i = 0; i < chans_to_copy; i++) {
                // does fan-out if 1 input channel
                block_copy(out_samps, src);
                src += stride;
                out_samps += BL;
            }
            // zero-pad input to fill out remaining channels
            if (chans_to_copy < chans) {
                block_zero_n(out_samps, chans - chans_to_copy);
            }
        }
    }
};
