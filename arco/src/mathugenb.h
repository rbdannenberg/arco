/* mathb -- unit generator for arco for binary mathb operations
 * Roger B. Dannenberg
 * Nov, 2023
 */


extern const char *Mathb_name;

typedef struct Mathb_state {
    Sample prev;
    Sample hold;
} Mathb_state;

class Mathb : public Ugen {
public:
    int op;
    Vec<Mathb_state> states;
    
    Ugen_ptr x1;
    int x1_stride;
    Sample_ptr x1_samps;

    Ugen_ptr x2;
    int x2_stride;
    Sample_ptr x2_samps;


    Mathb(int id, int nchans, int op_, Ugen_ptr x1_, Ugen_ptr x2_) :
            Ugen(id, 'b', nchans) {
        op = op_;
        if (op < 0) op = 0;
        if (op >= NUM_MATH_OPS) op = NUM_MATH_OPS - 1;
        x1 = x1_;
        x2 = x2_;
        flags = CAN_TERMINATE;
        states.set_size(chans);  // note that states are initialized to all zero

        init_x1(x1);
        init_x2(x2);
    }

    ~Mathb() {
        x1->unref();
        x2->unref();
    }

    const char *classname() { return Mathb_name; }

    void print_sources(int indent, bool print_flag) {
        x1->print_tree(indent, print_flag, "x1");
        x2->print_tree(indent, print_flag, "x2");
    }

    void repl_x1(Ugen_ptr ugen) {
        x1->unref();
        init_x1(ugen);
    }

    void repl_x2(Ugen_ptr ugen) {
        x2->unref();
        init_x2(ugen);
    }

    void set_x1(int chan, float f) {
        x1->const_set(chan, f, "Mathb::set_x1");
    }

    void set_x2(int chan, float f) {
        x2->const_set(chan, f, "Mathb::set_x2");
    }

    void init_x1(Ugen_ptr ugen) { init_param(ugen, x1, x1_stride); }

    void init_x2(Ugen_ptr ugen) { init_param(ugen, x2, x2_stride); }

    void real_run() {
        x1_samps = x1->run(current_block); // update input
        x2_samps = x2->run(current_block); // update input
        if (((x1->flags | x2->flags) & TERMINATED) &&
            (flags & CAN_TERMINATE)) {
            terminate();
        }
        for (int i = 0; i < chans; i++) {
            switch (op) {
              case MATH_OP_MUL:
                *out_samps++ = *x1_samps * *x2_samps;
                break;
              case MATH_OP_ADD:
                *out_samps++ = *x1_samps + *x2_samps;
                break;
              case MATH_OP_SUB:
                *out_samps++ = *x1_samps - *x2_samps;
                break;
              case MATH_OP_DIV: {
                Sample div = *x2_samps;
                div = copysign(fmaxf(fabsf(div), 0.01), div);
                *out_samps++ = *x1_samps / div; }
                break;
              case MATH_OP_MAX:
                *out_samps++ = fmaxf(*x1_samps, *x2_samps);
                break;
              case MATH_OP_MIN:
                *out_samps++ = fminf(*x1_samps, *x2_samps);
                break;
              case MATH_OP_CLP: {
                Sample x1 = *x1_samps;
                *out_samps++ = copysign(fminf(fabsf(x1), *x2_samps), x1); }
                break;
              case MATH_OP_POW:
                *out_samps++ = pow(*x1_samps, *x2_samps);
                break;
              case MATH_OP_LT:
                *out_samps++ = float(*x1_samps < *x2_samps);
                break;
              case MATH_OP_GT:
                *out_samps++ = float(*x1_samps > *x2_samps);
                break;
              case MATH_OP_SCP: {
                    Sample x1 = *x1_samps;
                    Sample x2 = *x2_samps;
                    *out_samps++ = SOFTCLIP(x1, x2);
                }
                break;
              case MATH_OP_PWI: {
                    Sample x1 = *x1_samps;
                    int power = round(*x2_samps);
                    Sample y = pow(fabsf(x1), power);
                    *out_samps++ = (power & 1) ? copysign(y, x1) : x1;
                }
                break;
              case MATH_OP_RND:
                *out_samps++ = unifrand_range(*x1_samps, *x2_samps);
                break;
                    
              case MATH_OP_SH: {
                    Mathb_state *state = &states[i];
                    Sample h = state->hold;
                    Sample p = state->prev;
                    Sample x2 = x2_samps[i];
                    if (p <= 0 && x2 > 0) {
                        h = *x1_samps;
                        state->hold = h;
                    }
                    *out_samps++ = h;
                    state->prev = x2;
                }
                break;
              default: break;
            }
            x1_samps += x1_stride;
            x2_samps += x2_stride;
        }
    }
};
