/* unaryb -- unit generator for arco for binary unaryb operations
 * Roger B. Dannenberg
 * Nov, 2023
 */


extern const char *Unaryb_name;

typedef struct Unaryb_state {
    int count;
    Sample prev;
    Sample hold;
} Unaryb_state;

class Unaryb : public Ugen {
public:
    int op;
    Vec<Unaryb_state> states;
    
    Ugen_ptr x1;
    int x1_stride;
    Sample_ptr x1_samps;


    Unaryb(int id, int nchans, int op_, Ugen_ptr x1_) :
            Ugen(id, 'b', nchans) {
        op = op_;
        if (op < 0) op = 0;
        if (op >= NUM_UNARY_OPS) op = UNARY_OP_ABS;
        x1 = x1_;
        flags = CAN_TERMINATE;
        states.set_size(chans);  // note that states are initialized to all zero

        init_x1(x1);
    }

    ~Unaryb() {
        x1->unref();
    }

    const char *classname() { return Unaryb_name; }

    void print_sources(int indent, bool print_flag) {
        x1->print_tree(indent, print_flag, "x1");
    }

    void repl_x1(Ugen_ptr ugen) {
        x1->unref();
        init_x1(ugen);
    }

    void set_x1(int chan, float f) {
        x1->const_set(chan, f, "Unaryb::set_x1");
    }

    void init_x1(Ugen_ptr ugen) { init_param(ugen, x1, &x1_stride); }

    void real_run() {
        x1_samps = x1->run(current_block); // update input
        if ((x1->flags & TERMINATED) &&
            (flags & CAN_TERMINATE)) {
            terminate(ACTION_TERM);
        }
        for (int i = 0; i < chans; i++) {
            switch (op) {
              case UNARY_OP_ABS:
                *out_samps++ = fabs(*x1_samps);
                break;
              case UNARY_OP_NEG:
                *out_samps++ = -*x1_samps;
                break;
              case UNARY_OP_EXP:
                *out_samps++ = expf(*x1_samps);
                break;
              case UNARY_OP_LOG:
                *out_samps++ = logf(*x1_samps);
                break;
              case UNARY_OP_LOG10:
                *out_samps++ = log10f(*x1_samps);
                break;
              case UNARY_OP_LOG2:
                *out_samps++ = log2f(*x1_samps);
                break;
              case UNARY_OP_SQRT:
                *out_samps++ = sqrtf(*x1_samps);
                break;
              case UNARY_OP_STEP_TO_HZ:
                *out_samps++ = step_to_hz(*x1_samps);
                break;
              case UNARY_OP_HZ_TO_STEP:
                *out_samps++ = hz_to_step(*x1_samps);
                break;
              case UNARY_OP_DB_TO_LINEAR:
                *out_samps++ = db_to_linear(*x1_samps);
                break;
              case UNARY_OP_LINEAR_TO_DB:
                *out_samps++ = linear_to_db(*x1_samps);
                break;
              default: break;
            }
            x1_samps += x1_stride;
        }
    }
};
