/* unary -- unit generator for arco
 *
 * Roger B. Dannenberg
 * Nov, 2023
 */

#include "arcougen.h"
#include "unaryugen.h"
#include "fastrand.h"

const char *Unary_name = "Unary";

//----------------- ABS ------------------

void Unary::abs_a_a(Unary_state *state)
{
    for (int i = 0; i < BL; i = i + 1) {
        *out_samps++ = fabs(x1_samps[i]);
    }
}

void Unary::abs_b_a(Unary_state *state)
{
    Sample br_sig = fabs(*x1_samps);
    Sample br_sig_fast = state->prev;
    Sample br_sig_incr = (br_sig - br_sig_fast) * BL_RECIP;
    state->prev = br_sig;
    for (int i = 0; i < BL; i = i + 1) {
        br_sig_fast += br_sig_incr;
        *out_samps++ = br_sig_fast;
    }
}


//----------------- NEG ------------------

void Unary::neg_a_a(Unary_state *state)
{
    for (int i = 0; i < BL; i = i + 1) {
        *out_samps++ = -x1_samps[i];
    }
}

void Unary::neg_b_a(Unary_state *state)
{
    Sample br_sig = -*x1_samps;
    Sample br_sig_fast = state->prev;
    Sample br_sig_incr = (br_sig - br_sig_fast) * BL_RECIP;
    state->prev = br_sig;
    for (int i = 0; i < BL; i = i + 1) {
        br_sig_fast += br_sig_incr;
        *out_samps++ = br_sig_fast;
    }
}


//----------------- EXP ------------------

void Unary::exp_a_a(Unary_state *state)
{
    for (int i = 0; i < BL; i = i + 1) {
        *out_samps++ = expf(x1_samps[i]);
    }
}

void Unary::exp_b_a(Unary_state *state)
{
    Sample br_sig = expf(*x1_samps);
    Sample br_sig_fast = state->prev;
    Sample br_sig_incr = (br_sig - br_sig_fast) * BL_RECIP;
    state->prev = br_sig;
    for (int i = 0; i < BL; i = i + 1) {
        br_sig_fast += br_sig_incr;
        *out_samps++ = br_sig_fast;
    }
}

//----------------- LOG ------------------

void Unary::log_a_a(Unary_state *state)
{
    for (int i = 0; i < BL; i = i + 1) {
        *out_samps++ = logf(x1_samps[i]);
    }
}

void Unary::log_b_a(Unary_state *state)
{
    Sample br_sig = logf(*x1_samps);
    Sample br_sig_fast = state->prev;
    Sample br_sig_incr = (br_sig - br_sig_fast) * BL_RECIP;
    state->prev = br_sig;
    for (int i = 0; i < BL; i = i + 1) {
        br_sig_fast += br_sig_incr;
        *out_samps++ = br_sig_fast;
    }
}

//----------------- LOG10 ------------------

void Unary::log10_a_a(Unary_state *state)
{
    for (int i = 0; i < BL; i = i + 1) {
        *out_samps++ = log10f(x1_samps[i]);
    }
}

void Unary::log10_b_a(Unary_state *state)
{
    Sample br_sig = log10f(*x1_samps);
    Sample br_sig_fast = state->prev;
    Sample br_sig_incr = (br_sig - br_sig_fast) * BL_RECIP;
    state->prev = br_sig;
    for (int i = 0; i < BL; i = i + 1) {
        br_sig_fast += br_sig_incr;
        *out_samps++ = br_sig_fast;
    }
}

//----------------- LOG2 ------------------

void Unary::log2_a_a(Unary_state *state)
{
    for (int i = 0; i < BL; i = i + 1) {
        *out_samps++ = log2f(x1_samps[i]);
    }
}

void Unary::log2_b_a(Unary_state *state)
{
    Sample br_sig = log2f(*x1_samps);
    Sample br_sig_fast = state->prev;
    Sample br_sig_incr = (br_sig - br_sig_fast) * BL_RECIP;
    state->prev = br_sig;
    for (int i = 0; i < BL; i = i + 1) {
        br_sig_fast += br_sig_incr;
        *out_samps++ = br_sig_fast;
    }
}

//----------------- SQRT ------------------

void Unary::sqrt_a_a(Unary_state *state)
{
    for (int i = 0; i < BL; i = i + 1) {
        *out_samps++ = sqrtf(x1_samps[i]);
    }
}

void Unary::sqrt_b_a(Unary_state *state)
{
    Sample br_sig = sqrtf(*x1_samps);
    Sample br_sig_fast = state->prev;
    Sample br_sig_incr = (br_sig - br_sig_fast) * BL_RECIP;
    state->prev = br_sig;
    for (int i = 0; i < BL; i = i + 1) {
        br_sig_fast += br_sig_incr;
        *out_samps++ = br_sig_fast;
    }
}

//----------------- STEP_TO_HZ ------------------

void Unary::step_to_hz_a_a(Unary_state *state)
{
    for (int i = 0; i < BL; i = i + 1) {
        *out_samps++ = step_to_hz(x1_samps[i]);
    }
}

void Unary::step_to_hz_b_a(Unary_state *state)
{
    Sample br_sig = step_to_hz(*x1_samps);
    Sample br_sig_fast = state->prev;
    Sample br_sig_incr = (br_sig - br_sig_fast) * BL_RECIP;
    state->prev = br_sig;
    for (int i = 0; i < BL; i = i + 1) {
        br_sig_fast += br_sig_incr;
        *out_samps++ = br_sig_fast;
    }
}


//----------------- HZ_TO_STEP ------------------

void Unary::hz_to_step_a_a(Unary_state *state)
{
    for (int i = 0; i < BL; i = i + 1) {
        *out_samps++ = hz_to_step(x1_samps[i]);
    }
}

void Unary::hz_to_step_b_a(Unary_state *state)
{
    Sample br_sig = hz_to_step(*x1_samps);
    Sample br_sig_fast = state->prev;
    Sample br_sig_incr = (br_sig - br_sig_fast) * BL_RECIP;
    state->prev = br_sig;
    for (int i = 0; i < BL; i = i + 1) {
        br_sig_fast += br_sig_incr;
        *out_samps++ = br_sig_fast;
    }
}


//----------------- VEL_TO_LINEAR ------------------

void Unary::vel_to_linear_a_a(Unary_state *state)
{
    for (int i = 0; i < BL; i = i + 1) {
        *out_samps++ = vel_to_linear(x1_samps[i]);
    }
}

void Unary::vel_to_linear_b_a(Unary_state *state)
{
    Sample br_sig = vel_to_linear(*x1_samps);
    Sample br_sig_fast = state->prev;
    Sample br_sig_incr = (br_sig - br_sig_fast) * BL_RECIP;
    state->prev = br_sig;
    for (int i = 0; i < BL; i = i + 1) {
        br_sig_fast += br_sig_incr;
        *out_samps++ = br_sig_fast;
    }
}


//----------------- LINEAR_TO_VEL ------------------

void Unary::linear_to_vel_a_a(Unary_state *state)
{
    for (int i = 0; i < BL; i = i + 1) {
        *out_samps++ = linear_to_vel(x1_samps[i]);
    }
}

void Unary::linear_to_vel_b_a(Unary_state *state)
{
    Sample br_sig = linear_to_vel(*x1_samps);
    Sample br_sig_fast = state->prev;
    Sample br_sig_incr = (br_sig - br_sig_fast) * BL_RECIP;
    state->prev = br_sig;
    for (int i = 0; i < BL; i = i + 1) {
        br_sig_fast += br_sig_incr;
        *out_samps++ = br_sig_fast;
    }
}


//----------------- DB_TO_LINEAR ------------------

void Unary::db_to_linear_a_a(Unary_state *state)
{
    for (int i = 0; i < BL; i = i + 1) {
        *out_samps++ = db_to_linear(x1_samps[i]);
    }
}

void Unary::db_to_linear_b_a(Unary_state *state)
{
    Sample br_sig = db_to_linear(*x1_samps);
    Sample br_sig_fast = state->prev;
    Sample br_sig_incr = (br_sig - br_sig_fast) * BL_RECIP;
    state->prev = br_sig;
    for (int i = 0; i < BL; i = i + 1) {
        br_sig_fast += br_sig_incr;
        *out_samps++ = br_sig_fast;
    }
}


//----------------- LINEAR_TO_DB ------------------

void Unary::linear_to_db_a_a(Unary_state *state)
{
    for (int i = 0; i < BL; i = i + 1) {
        *out_samps++ = linear_to_db(x1_samps[i]);
    }
}

void Unary::linear_to_db_b_a(Unary_state *state)
{
    Sample br_sig = linear_to_db(*x1_samps);
    Sample br_sig_fast = state->prev;
    Sample br_sig_incr = (br_sig - br_sig_fast) * BL_RECIP;
    state->prev = br_sig;
    for (int i = 0; i < BL; i = i + 1) {
        br_sig_fast += br_sig_incr;
        *out_samps++ = br_sig_fast;
    }
}


void (Unary::*run_a_a[NUM_UNARY_OPS])(Unary_state *state) = {
    &Unary::abs_a_a, &Unary::neg_a_a, &Unary::exp_a_a, &Unary::log_a_a, 
    &Unary::log10_a_a, &Unary::log2_a_a, &Unary::sqrt_a_a,
    &Unary::step_to_hz_a_a, &Unary::hz_to_step_a_a, 
    &Unary::vel_to_linear_a_a, &Unary::linear_to_vel_a_a,
    &Unary::db_to_linear_a_a, &Unary::linear_to_db_a_a };
void (Unary::*run_b_a[NUM_UNARY_OPS])(Unary_state *state) = {
    &Unary::abs_b_a, &Unary::neg_b_a, &Unary::exp_b_a, &Unary::log_b_a, 
    &Unary::log10_b_a, &Unary::log2_b_a, &Unary::sqrt_b_a,
    &Unary::step_to_hz_b_a, &Unary::hz_to_step_b_a, 
    &Unary::vel_to_linear_b_a, &Unary::linear_to_vel_b_a,
    &Unary::db_to_linear_b_a, &Unary::linear_to_db_b_a };


/* O2SM INTERFACE: /arco/unary/new int32 id, int32 chans, 
                                  int32 op, int32 x1;
 */
void arco_unary_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chans = argv[1]->i;
    int32_t op = argv[2]->i;
    int32_t x1 = argv[3]->i;
    // end unpack message

    ANY_UGEN_FROM_ID(x1_ugen,x1, "arco_unary_new");

    new Unary(id, chans, op, x1_ugen);
}


/* O2SM INTERFACE: /arco/unary/repl_x1 int32 id, int32 x1_id;
 */
static void arco_unary_repl_x1(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t x1_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Unary, unary, id, "arco_unary_repl_x1");
    ANY_UGEN_FROM_ID(x1, x1_id, "arco_unary_repl_x1");
    unary->repl_x1(x1);
}


/* O2SM INTERFACE: /arco/unary/set_x1 int32 id, int32 chan, float val;
 */
static void arco_unary_set_x1(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chan = argv[1]->i;
    float val = argv[2]->f;
    // end unpack message

    UGEN_FROM_ID(Unary, unary, id, "arco_unary_set_x1");
    unary->set_x1(chan, val);
}

static void unary_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/unary/new", "iiii", arco_unary_new, NULL, true,
                    true);
    o2sm_method_new("/arco/unary/repl_x1", "ii", arco_unary_repl_x1, NULL,
                    true, true);
    o2sm_method_new("/arco/unary/set_x1", "iif", arco_unary_set_x1, NULL,
                    true, true);
    // END INTERFACE INITIALIZATION

    // class initialization code from faust:
}

Initializer unary_init_obj(unary_init);
