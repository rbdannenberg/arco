/* mathb -- unit generator for arco for binary mathb operations
 * Roger B. Dannenberg
 * Nov, 2023
 */


extern const char *Mathb_name;

typedef struct Mathb_state {
    int count;
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
        if (op >= NUM_MATH_OPS) op = MATH_OP_ADD;
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

    void clear_counts() {
        // since we have a new parameter, we clear the count so that we will
        // immediately start ramping to a value between -x2 and x2 rather than
        // possibly wait for a long ramp to finish. A potential problem is that
        // if x1 is now low, creating long ramp times, and our current ramp value
        // (state->hold) is much larger than a new x2, it could take a long time
        // for the output to get within the desired range -x2 to x2.  It takes
        // a little work to determine what is affected by a change since an input
        // could have fanout to multiple output channels, so we just restart ramps
        // on all channels.
        if (op == MATH_OP_RLI) {
            for (int i = 0; i < chans; i++) {
                states[i].count = 0;
            }
        }
    }


    void repl_x1(Ugen_ptr ugen) {
        x1->unref();
        init_x1(ugen);
        clear_counts();
    }

    void repl_x2(Ugen_ptr ugen) {
        x2->unref();
        init_x2(ugen);
        clear_counts();
    }

    void set_x1(int chan, float f) {
        x1->const_set(chan, f, "Mathb::set_x1");
        clear_counts();
    }

    void set_x2(int chan, float f) {
        x2->const_set(chan, f, "Mathb::set_x2");
        clear_counts();
    }

    void rliset(float f) {
        if (op == MATH_OP_RLI) {
            for (int i = 0; i < chans; i++) {
                states[i].prev = unifrand_range(-f, f);
            }
        }
    }

    void init_x1(Ugen_ptr ugen) { init_param(ugen, x1, &x1_stride); }

    void init_x2(Ugen_ptr ugen) { init_param(ugen, x2, &x2_stride); }

    void real_run() {
        x1_samps = x1->run(current_block); // update input
        x2_samps = x2->run(current_block); // update input
        if (((x1->flags | x2->flags) & TERMINATED) && (flags & CAN_TERMINATE)) {
            terminate(ACTION_TERM);
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
                    Sample x2 = *x2_samps;
                    if (p <= 0 && x2 > 0) {
                        h = *x1_samps;
                        state->hold = h;
                    }
                    *out_samps++ = h;
                    state->prev = x2;
                }
                break;

              case MATH_OP_QNT: {
                    Sample x2 = *x2_samps;
                    Sample q = x2 * 0x8000;
                    *out_samps++ = x2 <= 0 ? 0 : 
                            round((*x1_samps + 1) * q) / q - 1;
                }
                break;

              case MATH_OP_RLI: {
                    Mathb_state *state = &states[i];
                    if (state->count == 0) {
                        state->count = (int) (BR / *x1_samps);
                        if (state->count == 0) {
                            state->count = 1;
                        }
                        Sample x2 = *x2_samps;
                        Sample target = unifrand_range(-x2, x2);
                        state->hold = (target - state->prev) / state->count;
                    }
                    state->count--;
                    *out_samps++ = (state->prev += state->hold);
                }
                break;

              case MATH_OP_HZDIFF:
                *out_samps = (float) steps_to_hzdiff(*x1_samps, *x2_samps);
                break;

              case MATH_OP_TAN: {
                    *out_samps++ = tanf(*x1_samps * *x2_samps);
                }
                break;

              case MATH_OP_ATAN2: {
                    *out_samps++ = atan2f(*x1_samps, *x2_samps);
                }
                break;

              case MATH_OP_SIN: {
                    *out_samps++ = sinf(*x1_samps * *x2_samps);
                }
                break;

              case MATH_OP_COS: {
                    *out_samps++ = cosf(*x1_samps * *x2_samps);
                }
                break;

              default: break;
            }
            x1_samps += x1_stride;
            x2_samps += x2_stride;
        }
    }
};
