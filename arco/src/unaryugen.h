/* unaryugen -- unit generator for arco for unary math operations
 *
 */

#include <algorithm>
#include <cmath>
#include <cstdint>

#ifdef __APPLE__ 
#define exp10f __exp10f
#define exp10 __exp10
#endif

/*-------------- END FAUST PREAMBLE --------------*/

extern const char *Unary_name;

typedef struct Unary_state {
    Sample prev;
} Unary_state;

class Unary;

extern void (Unary::*run_a_a[NUM_UNARY_OPS])(Unary_state *state);
extern void (Unary::*run_b_a[NUM_UNARY_OPS])(Unary_state *state);

class Unary : public Ugen {
public:
    int op;
    Vec<Unary_state> states;
    void (Unary::*run_channel)(Unary_state *state);

    Ugen_ptr x1;
    int x1_stride;
    Sample_ptr x1_samps;

    Unary(int id, int nchans, int op_, Ugen_ptr x1_) :
            Ugen(id, 'a', nchans) {
        op = op_;
        if (op < 0) op = 0;
        if (op >= NUM_UNARY_OPS) op = UNARY_OP_ABS;
        x1 = x1_;
        flags = CAN_TERMINATE;
        states.set_size(chans);

        init_x1(x1);
        run_channel = (void (Unary::*)(Unary_state *)) 0;
        update_run_channel();
    }

    ~Unary() {
        x1->unref();
    }

    const char *classname() { return Unary_name; }

    void initialize_channel_states() {
        for (int i = 0; i < chans; i++) {
            states[i].prev = 0.0f;
        }
    }

    void update_run_channel() {
        // initialize run_channel based on input types
        void (Unary::*new_run_channel)(Unary_state *state);
        if (x1->rate == 'a') {
            new_run_channel = run_a_a[op];
        } else {
            new_run_channel = run_b_a[op];
        }
        if (new_run_channel != run_channel) {
            initialize_channel_states();
            run_channel = new_run_channel;
        }
    }

    void print_details(int indent) {
        arco_print("op %s ", UNARY_OP_TO_STRING[op]);
    }
    
    void print_sources(int indent, bool print_flag) {
        x1->print_tree(indent, print_flag, "x1");
    }

    void repl_x1(Ugen_ptr ugen) {
        x1->unref();
        init_x1(ugen);
        update_run_channel();
    }

    void set_x1(int chan, float f) {
        x1->const_set(chan, f, "Unary::set_x1");
    }

    void init_x1(Ugen_ptr ugen) { init_param(ugen, x1, &x1_stride); }

    //----------------- ABS ------------------

    void abs_a_a(Unary_state *state);
    void abs_b_a(Unary_state *state);

    //----------------- NEG ------------------

    void neg_a_a(Unary_state *state);
    void neg_b_a(Unary_state *state);

    //----------------- EXP ------------------

    void exp_a_a(Unary_state *state);
    void exp_b_a(Unary_state *state);

    //----------------- LOG ------------------

    void log_a_a(Unary_state *state);
    void log_b_a(Unary_state *state);

    //----------------- LOG10 ------------------

    void log10_a_a(Unary_state *state);
    void log10_b_a(Unary_state *state);

    //----------------- LOG2 ------------------

    void log2_a_a(Unary_state *state);
    void log2_b_a(Unary_state *state);

    //----------------- SQRT ------------------

    void sqrt_a_a(Unary_state *state);
    void sqrt_b_a(Unary_state *state);

    //----------------- STEP_TO_HZ ------------------

    void step_to_hz_a_a(Unary_state *state);
    void step_to_hz_b_a(Unary_state *state);

    //----------------- HZ_TO_STEP ------------------

    void hz_to_step_a_a(Unary_state *state);
    void hz_to_step_b_a(Unary_state *state);

    //----------------- VEL_TO_LINEAR ------------------

    void vel_to_linear_a_a(Unary_state *state);
    void vel_to_linear_b_a(Unary_state *state);

    //----------------- LINEAR_TO_VEL ------------------

    void linear_to_vel_a_a(Unary_state *state);
    void linear_to_vel_b_a(Unary_state *state);


    //----------------- DB_TO_LINEAR ------------------

    void db_to_linear_a_a(Unary_state *state);
    void db_to_linear_b_a(Unary_state *state);

    //----------------- LINEAR_TO_DB ------------------

    void linear_to_db_a_a(Unary_state *state);
    void linear_to_db_b_a(Unary_state *state);


    void real_run() {
        x1_samps = x1->run(current_block); // update input
        if ((x1->flags & TERMINATED) &&
            (flags & CAN_TERMINATE)) {
            terminate(ACTION_TERM);
        }
        Unary_state *state = &states[0];
        for (int i = 0; i < chans; i++) {
            (this->*run_channel)(state);
            state++;
            x1_samps += x1_stride;
        }
    }
};



