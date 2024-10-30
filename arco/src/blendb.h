/* blendb -- unit generator for arco
 *
 * adapted from multx, Oct 2024
 *
 * blendb is a selector using a signal to select or mix 2 inputs
 *
 */

#ifndef  __Blendb_H__
#define  __Blendb_H__

const int BLENDB_LINEAR = 0;
const int BLENDB_POWER = 1;
const int BLENDB_45 = 2;

extern const char *Blendb_name;

class Blendb : public Ugen {
public:
    Ugen_ptr x1;
    Sample *x1_samps;
    int x1_stride;
    
    Ugen_ptr x2;
    Sample *x2_samps;
    int x2_stride;
    
    Ugen_ptr b;
    Sample *b_samps;
    int b_stride;
    
    float gain;
    int mode;

    Blendb(int id, int nchans, Ugen_ptr x1_, Ugen_ptr x2_, Ugen_ptr b_, 
           int mode_) :
            Ugen(id, 'a', nchans) {
        x1 = x1_;
        x2 = x2_;
        b = b_;
        mode = mode_;
        gain = 1;
        flags = CAN_TERMINATE;

        init_x1(x1);
        init_x2(x2);
        init_b(b);
    }

    ~Blendb() {
        x1->unref();
        x2->unref();
        b->unref();
    }

    const char *classname() { return Blendb_name; }

    void print_sources(int indent, bool print_flag) {
        x1->print_tree(indent, print_flag, "x1");
        x2->print_tree(indent, print_flag, "x2");
        b->print_tree(indent, print_flag, "b");
    }

    void repl_x1(Ugen_ptr ugen) {
        x1->unref();
        init_x1(ugen);
    }

    void repl_x2(Ugen_ptr ugen) {
        x2->unref();
        init_x2(ugen);
    }

    void repl_b(Ugen_ptr ugen) {
        b->unref();
        init_b(ugen);
    }

    void set_x1(int chan, float f) {
        x1->const_set(chan, f, "Blendb::set_x1");
    }

    void set_x2(int chan, float f) {
        x2->const_set(chan, f, "Blendb::set_x2");
    }

    void set_b(int chan, float f) {
        b->const_set(chan, f, "Blendb::set_x2");
    }

    void init_x1(Ugen_ptr ugen) {
        if (ugen->rate == 'a') {
            ugen = new Dnsampleb(-1, ugen->chans, ugen, LOWPASS500);
        }
        init_param(ugen, x1, &x1_stride); 
    }

    void init_x2(Ugen_ptr ugen) {
        if (ugen->rate == 'a') {
            ugen = new Dnsampleb(-1, ugen->chans, ugen, LOWPASS500);
        }
        init_param(ugen, x2, &x2_stride);
    }

    void init_b(Ugen_ptr ugen) {
        if (ugen->rate == 'a') {
            ugen = new Dnsampleb(-1, ugen->chans, ugen, LOWPASS500);
        }
        init_param(ugen, b, &b_stride); 
    }

    
    void real_run() {
        x1_samps = x1->run(current_block); // update input
        x2_samps = x2->run(current_block); // update input
        b_samps = b->run(current_block);  // update input
        if ((x1->flags & x2->flags & TERMINATED) &&
            (flags & CAN_TERMINATE)) {
            terminate(ACTION_TERM);
        }
        float b = *b_samps;
        if (mode != BLEND_LINEAR) {
            float angle = (COS_TABLE_SIZE + 2) - ((b) * (COS_TABLE_SIZE / 2.0));
            int anglei = (int) angle;
            float x1_gain = raised_cosine[anglei];
            x1_gain += (angle - anglei) * (x1_gain - raised_cosine[anglei + 1]);
            x1_gain += x1_gain - 1;  /* convert raised cos to cos */
            angle = (COS_TABLE_SIZE * 3 / 2.0f) - angle;
            anglei = (int) angle;
            float x2_gain = raised_cosine[anglei];
            x2_gain += (angle - anglei) * (x2_gain - raised_cosine[anglei + 1]);
            x2_gain += x2_gain - 1;
            if (mode == BLEND_45) { // blendb linear with constant power:
                x1_gain = sqrt((1 - b) * x1_gain);
                x2_gain = sqrt((1 - b) * x2_gain);
            }
            *out_samps++ = gain * (*x1_samps * x1_gain + *x2_samps * x2_gain);
        } else {
            *out_samps++ = gain * (*x1_samps * (1.0f - b) + *x2_samps * b);
        }
        x1_samps += x1_stride;
        x2_samps += x2_stride;
        b_samps += b_stride;
    }
};
#endif
