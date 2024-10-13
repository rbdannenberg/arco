/* math -- unit generator for arco
 *
 * Roger B. Dannenberg
 * Nov, 2023
 */

#include "arcougen.h"
#include "mathugen.h"
#include "fastrand.h"

const char *Math_name = "Math";

//----------------- mul ------------------

void Math::mul_aa_a(Math_state *state)
{
    for (int i = 0; i < BL; i = i + 1) {
        *out_samps++ = x1_samps[i] * x2_samps[i];
    }
}

void Math::mul_ab_a(Math_state *state)
{
    Sample br_sig = *x2_samps;
    Sample br_sig_fast = state->prev;
    Sample br_sig_incr = (br_sig - br_sig_fast) * BL_RECIP;
    state->prev = br_sig;
    for (int i = 0; i < BL; i = i + 1) {
        br_sig_fast += br_sig_incr;
        *out_samps++ = br_sig_fast * x1_samps[i];
    }
}

void Math::mul_ba_a(Math_state *state)
{
    Sample br_sig = *x1_samps;
    Sample br_sig_fast = state->prev;
    Sample br_sig_incr = (br_sig - br_sig_fast) * BL_RECIP;
    state->prev = br_sig;
    for (int i = 0; i < BL; i = i + 1) {
        br_sig_fast += br_sig_incr;
        *out_samps++ = br_sig_fast * x2_samps[i];
    }
}

void Math::mul_bb_a(Math_state *state)
{
    Sample br_sig = *x1_samps * *x2_samps;
    Sample br_sig_fast = state->prev;
    Sample br_sig_incr = (br_sig - br_sig_fast) * BL_RECIP;
    state->prev = br_sig;
    for (int i = 0; i < BL; i = i + 1) {
        br_sig_fast += br_sig_incr;
        *out_samps++ = br_sig_fast;
    }
}

//----------------- add ------------------

void Math::add_aa_a(Math_state *state)
{
    for (int i = 0; i < BL; i = i + 1) {
        *out_samps++ = x1_samps[i] + x2_samps[i];
    }
}

void Math::add_ab_a(Math_state *state)
{
    Sample br_sig = *x2_samps;
    Sample br_sig_fast = state->prev;
    Sample br_sig_incr = (br_sig - br_sig_fast) * BL_RECIP;
    state->prev = br_sig;
    for (int i = 0; i < BL; i = i + 1) {
        br_sig_fast += br_sig_incr;
        *out_samps++ = br_sig_fast + x1_samps[i];
    }
}

void Math::add_ba_a(Math_state *state)
{
    Sample br_sig = *x1_samps;
    Sample br_sig_fast = state->prev;
    Sample br_sig_incr = (br_sig - br_sig_fast) * BL_RECIP;
    state->prev = br_sig;
    for (int i = 0; i < BL; i = i + 1) {
        br_sig_fast += br_sig_incr;
        *out_samps++ = br_sig_fast + x2_samps[i];
    }
}

void Math::add_bb_a(Math_state *state)
{
    Sample br_sig = *x1_samps + *x2_samps;
    Sample br_sig_fast = state->prev;
    Sample br_sig_incr = (br_sig - br_sig_fast) * BL_RECIP;
    state->prev = br_sig;
    for (int i = 0; i < BL; i = i + 1) {
        br_sig_fast += br_sig_incr;
        *out_samps++ = br_sig_fast;
    }
}

//----------------- sub ------------------

void Math::sub_aa_a(Math_state *state)
{
    for (int i = 0; i < BL; i = i + 1) {
        *out_samps++ = x1_samps[i] - x2_samps[i];
    }
}

void Math::sub_ab_a(Math_state *state)
{
    Sample br_sig = *x2_samps;
    Sample br_sig_fast = state->prev;
    Sample br_sig_incr = (br_sig - br_sig_fast) * BL_RECIP;
    state->prev = br_sig;
    for (int i = 0; i < BL; i = i + 1) {
        br_sig_fast += br_sig_incr;
        *out_samps++ = br_sig_fast - x1_samps[i];
    }
}

void Math::sub_ba_a(Math_state *state)
{
    Sample br_sig = *x1_samps;
    Sample br_sig_fast = state->prev;
    Sample br_sig_incr = (br_sig - br_sig_fast) * BL_RECIP;
    state->prev = br_sig;
    for (int i = 0; i < BL; i = i + 1) {
        br_sig_fast += br_sig_incr;
        *out_samps++ = br_sig_fast - x2_samps[i];
    }
}

void Math::sub_bb_a(Math_state *state)
{
    Sample br_sig = *x1_samps - *x2_samps;
    Sample br_sig_fast = state->prev;
    Sample br_sig_incr = (br_sig - br_sig_fast) * BL_RECIP;
    state->prev = br_sig;
    for (int i = 0; i < BL; i = i + 1) {
        br_sig_fast += br_sig_incr;
        *out_samps++ = br_sig_fast;
    }
}

//----------------- div ------------------
// div is division except we do not divide by anything smaller 
// than 0.01, so if |x2| < 0.01, divide by 0.01 or -0.01

void Math::div_aa_a(Math_state *state)
{
    for (int i = 0; i < BL; i = i + 1) {
        Sample div = x2_samps[i];
        div = copysign(fmaxf(fabsf(div), 0.01), div);
        *out_samps++ = x1_samps[i] / div;
    }
}

void Math::div_ab_a(Math_state *state)
{
    Sample br_sig = *x2_samps;
    // this is tricky because x2 might cross zero, and we do not
    // want to interpolate through zero. Instead, if we cross
    // zero, insert a discontinuity, jumping to either 0.01 or -0.01
    // to make prev have the same sign.
    Sample br_sig_abs = fabsf(br_sig);
    if (state->prev * br_sig_abs < 0) {  // signs don't match
        state->prev = copysign(0.01, br_sig);  // jump across zero
    }
    br_sig = copysign(fmaxf(br_sig_abs, 0.01), br_sig);
    Sample br_sig_fast = state->prev;
    Sample br_sig_incr = (br_sig - br_sig_fast) * BL_RECIP;
    state->prev = br_sig;
    for (int i = 0; i < BL; i = i + 1) {
        br_sig_fast += br_sig_incr;
        *out_samps++ = x1_samps[i] / br_sig_fast;
    }
}

void Math::div_ba_a(Math_state *state)
{
    Sample br_sig = *x1_samps;
    Sample br_sig_fast = state->prev;
    Sample br_sig_incr = (br_sig - br_sig_fast) * BL_RECIP;
    state->prev = br_sig;
    for (int i = 0; i < BL; i = i + 1) {
        br_sig_fast += br_sig_incr;
        Sample div = *x2_samps;
        div = copysign(fmaxf(fabsf(div), 0.01), div);
        *out_samps++ = br_sig_fast / div;
    }
}

void Math::div_bb_a(Math_state *state)
{
    Sample div = *x2_samps;
    div = copysign(fmaxf(fabsf(div), 0.01), div);
    Sample br_sig = *x1_samps / div;
    Sample br_sig_fast = state->prev;
    Sample br_sig_incr = (br_sig - br_sig_fast) * BL_RECIP;
    state->prev = br_sig;
    for (int i = 0; i < BL; i = i + 1) {
        br_sig_fast += br_sig_incr;
        *out_samps++ = br_sig_fast;
    }
}

//----------------- max ------------------

void Math::max_aa_a(Math_state *state)
{
    for (int i = 0; i < BL; i = i + 1) {
        *out_samps++ = fmaxf(x1_samps[i], x2_samps[i]);
    }
}

void Math::max_ab_a(Math_state *state)
{
    Sample br_sig = *x2_samps;
    Sample br_sig_fast = state->prev;
    Sample br_sig_incr = (br_sig - br_sig_fast) * BL_RECIP;
    state->prev = br_sig;
    for (int i = 0; i < BL; i = i + 1) {
        br_sig_fast += br_sig_incr;
        *out_samps++ = fmaxf(br_sig_fast, x1_samps[i]);
    }
}

void Math::max_ba_a(Math_state *state)
{
    Sample br_sig = *x1_samps;
    Sample br_sig_fast = state->prev;
    Sample br_sig_incr = (br_sig - br_sig_fast) * BL_RECIP;
    state->prev = br_sig;
    for (int i = 0; i < BL; i = i + 1) {
        br_sig_fast += br_sig_incr;
        *out_samps++ = fmaxf(br_sig_fast, x2_samps[i]);
    }
}

void Math::max_bb_a(Math_state *state)
{
    Sample br_sig = fmaxf(*x1_samps, *x2_samps);
    Sample br_sig_fast = state->prev;
    Sample br_sig_incr = (br_sig - br_sig_fast) * BL_RECIP;
    state->prev = br_sig;
    for (int i = 0; i < BL; i = i + 1) {
        br_sig_fast += br_sig_incr;
        *out_samps++ = br_sig_fast;
    }
}

//----------------- min ------------------

void Math::min_aa_a(Math_state *state)
{
    for (int i = 0; i < BL; i = i + 1) {
        *out_samps++ = fminf(x1_samps[i], x2_samps[i]);
    }
}

void Math::min_ab_a(Math_state *state)
{
    Sample br_sig = *x2_samps;
    Sample br_sig_fast = state->prev;
    Sample br_sig_incr = (br_sig - br_sig_fast) * BL_RECIP;
    state->prev = br_sig;
    for (int i = 0; i < BL; i = i + 1) {
        br_sig_fast += br_sig_incr;
        *out_samps++ = fminf(br_sig_fast, x1_samps[i]);
    }
}

void Math::min_ba_a(Math_state *state)
{
    Sample br_sig = *x1_samps;
    Sample br_sig_fast = state->prev;
    Sample br_sig_incr = (br_sig - br_sig_fast) * BL_RECIP;
    state->prev = br_sig;
    for (int i = 0; i < BL; i = i + 1) {
        br_sig_fast += br_sig_incr;
        *out_samps++ = fminf(br_sig_fast, x2_samps[i]);
    }
}

void Math::min_bb_a(Math_state *state)
{
    Sample br_sig = fminf(*x1_samps, *x2_samps);
    Sample br_sig_fast = state->prev;
    Sample br_sig_incr = (br_sig - br_sig_fast) * BL_RECIP;
    state->prev = br_sig;
    for (int i = 0; i < BL; i = i + 1) {
        br_sig_fast += br_sig_incr;
        *out_samps++ = br_sig_fast;
    }
}

//----------------- clp ------------------
// clip x1 to the range of (positive) x2

void Math::clp_aa_a(Math_state *state)
{
    for (int i = 0; i < BL; i = i + 1) {
        Sample x1 = x1_samps[i];
        Sample x2  =x2_samps[i];
        *out_samps++ = copysign(fminf(fabsf(x1), x2), x1);
    }
}

void Math::clp_ab_a(Math_state *state)
{
    Sample br_sig = *x2_samps;
    Sample br_sig_fast = state->prev;
    Sample br_sig_incr = (br_sig - br_sig_fast) * BL_RECIP;
    state->prev = br_sig;
    for (int i = 0; i < BL; i = i + 1) {
        br_sig_fast += br_sig_incr;
        Sample x1 = x1_samps[i];
        *out_samps++ = copysign(fminf(fabsf(x1), br_sig_fast), x1);
    }
}

void Math::clp_ba_a(Math_state *state)
{
    Sample br_sig = *x1_samps;
    Sample br_sig_fast = state->prev;
    Sample br_sig_incr = (br_sig - br_sig_fast) * BL_RECIP;
    state->prev = br_sig;
    for (int i = 0; i < BL; i = i + 1) {
        br_sig_fast += br_sig_incr;
        *out_samps++ = copysign(fminf(br_sig_fast, x2_samps[i]), br_sig_fast);
    }
}

void Math::clp_bb_a(Math_state *state)
{
    Sample x1 = *x1_samps;
    Sample br_sig = copysign(fminf(x1, *x2_samps), x1);
    Sample br_sig_fast = state->prev;
    Sample br_sig_incr = (br_sig - br_sig_fast) * BL_RECIP;
    state->prev = br_sig;
    for (int i = 0; i < BL; i = i + 1) {
        br_sig_fast += br_sig_incr;
        *out_samps++ = br_sig_fast;
    }
}

//----------------- pow ------------------

void Math::pow_aa_a(Math_state *state)
{
    for (int i = 0; i < BL; i = i + 1) {
        *out_samps++ = pow(x1_samps[i], x2_samps[i]);
    }
}

void Math::pow_ab_a(Math_state *state)
{
    Sample br_sig = *x2_samps;
    Sample br_sig_fast = state->prev;
    Sample br_sig_incr = (br_sig - br_sig_fast) * BL_RECIP;
    state->prev = br_sig;
    for (int i = 0; i < BL; i = i + 1) {
        br_sig_fast += br_sig_incr;
        Sample x1 = x1_samps[i];
        *out_samps++ = (x1 < 0 ? 0 : pow(x1, br_sig_fast));
    }
}

void Math::pow_ba_a(Math_state *state)
{
    Sample br_sig = *x1_samps;
    Sample br_sig_fast = state->prev;
    Sample br_sig_incr = (br_sig - br_sig_fast) * BL_RECIP;
    state->prev = br_sig;
    if (br_sig_fast < 0 || br_sig < 0) {
        block_zero(out_samps);
        out_samps += BL;
        return;
    }
    for (int i = 0; i < BL; i = i + 1) {
        br_sig_fast += br_sig_incr;
        *out_samps++ = pow(br_sig_fast, x2_samps[i]);
    }
}

void Math::pow_bb_a(Math_state *state)
{
    Sample x1 = *x1_samps;
    Sample br_sig = (x1 < 0 ? 0 : pow(x1, *x2_samps));
    Sample br_sig_fast = state->prev;
    Sample br_sig_incr = (br_sig - br_sig_fast) * BL_RECIP;
    state->prev = br_sig;
    for (int i = 0; i < BL; i = i + 1) {
        br_sig_fast += br_sig_incr;
        *out_samps++ = br_sig_fast;
    }
}

//----------------- LT ------------------

void Math::lt_aa_a(Math_state *state)
{
    for (int i = 0; i < BL; i = i + 1) {
        *out_samps++ = float(x1_samps[i] < x2_samps[i]);
    }
}

void Math::lt_ab_a(Math_state *state)
{
    Sample br_sig = *x2_samps;
    Sample br_sig_fast = state->prev;
    Sample br_sig_incr = (br_sig - br_sig_fast) * BL_RECIP;
    state->prev = br_sig;
    for (int i = 0; i < BL; i = i + 1) {
        br_sig_fast += br_sig_incr;
        *out_samps++ = float(br_sig_fast < x1_samps[i]);
    }
}

void Math::lt_ba_a(Math_state *state)
{
    Sample br_sig = *x1_samps;
    Sample br_sig_fast = state->prev;
    Sample br_sig_incr = (br_sig - br_sig_fast) * BL_RECIP;
    state->prev = br_sig;
    for (int i = 0; i < BL; i = i + 1) {
        br_sig_fast += br_sig_incr;
        *out_samps++ = float(br_sig_fast < x2_samps[i]);
    }
}

void Math::lt_bb_a(Math_state *state)
{
    Sample br_sig = float(*x1_samps < *x2_samps);
    Sample br_sig_fast = state->prev;
    Sample br_sig_incr = (br_sig - br_sig_fast) * BL_RECIP;
    state->prev = br_sig;
    for (int i = 0; i < BL; i = i + 1) {
        br_sig_fast += br_sig_incr;
        *out_samps++ = br_sig_fast;
    }
}

//----------------- GT ------------------

void Math::gt_aa_a(Math_state *state)
{
    for (int i = 0; i < BL; i = i + 1) {
        *out_samps++ = float(x1_samps[i] > x2_samps[i]);
    }
}

void Math::gt_ab_a(Math_state *state)
{
    Sample br_sig = *x2_samps;
    Sample br_sig_fast = state->prev;
    Sample br_sig_incr = (br_sig - br_sig_fast) * BL_RECIP;
    state->prev = br_sig;
    for (int i = 0; i < BL; i = i + 1) {
        br_sig_fast += br_sig_incr;
        *out_samps++ = float(br_sig_fast > x1_samps[i]);
    }
}

void Math::gt_ba_a(Math_state *state)
{
    Sample br_sig = *x1_samps;
    Sample br_sig_fast = state->prev;
    Sample br_sig_incr = (br_sig - br_sig_fast) * BL_RECIP;
    state->prev = br_sig;
    for (int i = 0; i < BL; i = i + 1) {
        br_sig_fast += br_sig_incr;
        *out_samps++ = float(br_sig_fast > x2_samps[i]);
    }
}

void Math::gt_bb_a(Math_state *state)
{
    Sample br_sig = float(*x1_samps > *x2_samps);
    Sample br_sig_fast = state->prev;
    Sample br_sig_incr = (br_sig - br_sig_fast) * BL_RECIP;
    state->prev = br_sig;
    for (int i = 0; i < BL; i = i + 1) {
        br_sig_fast += br_sig_incr;
        *out_samps++ = br_sig_fast;
    }
}

//----------------- SCP ------------------
// soft clipping y = x2 if x1 > x2; -x2 if x1 < -x2;
// o.w. y = ((x1 / x2) - ((x1 / x2) ** 3) / 3) * (3 / 2) * x2
//        = (x * 3 - x ** 3) * 0.5 * x2, where x = x1 / x2

void Math::scp_aa_a(Math_state *state)
{
    for (int i = 0; i < BL; i = i + 1) {
        Sample x1 = x1_samps[i];
        Sample x2 = x2_samps[i];
        *out_samps++ = SOFTCLIP(x1, x2);
    }
}

void Math::scp_ab_a(Math_state *state)
{
    Sample br_sig = *x2_samps;
    Sample br_sig_fast = state->prev;
    Sample br_sig_incr = (br_sig - br_sig_fast) * BL_RECIP;
    state->prev = br_sig;
    // this optimization avoids compare in inner loop, but does
    // fills the whole output with zeros when x2 (at block-rate)
    // crosses from below zero to above zero:
    if (br_sig < 0 || br_sig_fast < 0) {
        block_zero(out_samps);
        out_samps += BL;
        return;
    }
    for (int i = 0; i < BL; i = i + 1) {
        br_sig_fast += br_sig_incr;
        Sample x1 = x1_samps[i];
        *out_samps++ = SOFTCLIP(x1, br_sig_fast);
    }
}

void Math::scp_ba_a(Math_state *state)
{
    Sample br_sig = *x1_samps;
    Sample br_sig_fast = state->prev;
    Sample br_sig_incr = (br_sig - br_sig_fast) * BL_RECIP;
    state->prev = br_sig;
    for (int i = 0; i < BL; i = i + 1) {
        br_sig_fast += br_sig_incr;
        Sample x1 = br_sig_fast;
        Sample x2 = x2_samps[i];
        *out_samps++ = SOFTCLIP(x1, x2);
    }
}

void Math::scp_bb_a(Math_state *state)
{
    Sample x1 = *x1_samps;
    Sample x2 = *x2_samps;
    Sample br_sig = SOFTCLIP(x1, x2);
    Sample br_sig_fast = state->prev;
    Sample br_sig_incr = (br_sig - br_sig_fast) * BL_RECIP;
    state->prev = br_sig;
    for (int i = 0; i < BL; i = i + 1) {
        br_sig_fast += br_sig_incr;
        *out_samps++ = br_sig_fast;
    }
}

//----------------- pwi ------------------
// raise x1 to an integer x2

void Math::pwi_aa_a(Math_state *state)
{
    for (int i = 0; i < BL; i = i + 1) {
        Sample x1 = x1_samps[i];
        int power = round(x2_samps[i]);
        Sample y = pow(fabsf(x1), power);
        *out_samps++ = (power & 1) ? copysign(y, x1) : x1;
    }
}

void Math::pwi_ab_a(Math_state *state)
{
    Sample x2 = *x2_samps;
    int power = round(x2);
    if (power & 1) {  // power is odd, keep the sign of x1
        for (int i = 0; i < BL; i = i + 1) {
            Sample x1 = x1_samps[i];
            *out_samps++ = copysign(pow(fabsf(x1), power), x1);
        }
    } else {
        for (int i = 0; i < BL; i = i + 1) {
            *out_samps++ = pow(fabs(x1_samps[i]), power);
        }
    }
}

void Math::pwi_ba_a(Math_state *state)
{
    Sample br_sig = *x1_samps;
    Sample br_sig_fast = state->prev;
    Sample br_sig_incr = (br_sig - br_sig_fast) * BL_RECIP;
    state->prev = br_sig;
    for (int i = 0; i < BL; i = i + 1) {
        br_sig_fast += br_sig_incr;
        int power = round(x2_samps[i]);
        Sample y = pow(fabsf(br_sig_fast), power);
        *out_samps++ = (power & 1) ? copysign(y, br_sig_fast) : y;
    }
}

void Math::pwi_bb_a(Math_state *state)
{
    int power = round(*x2_samps);
    Sample x1 = *x1_samps;
    Sample br_sig = pow(fabs(x1), power);
    if (power & 1) {  // power is odd, keep the sign of x1
        br_sig = copysign(br_sig, x1);
    }
    Sample br_sig_fast = state->prev;
    Sample br_sig_incr = (br_sig - br_sig_fast) * BL_RECIP;
    state->prev = br_sig;
    for (int i = 0; i < BL; i = i + 1) {
        br_sig_fast += br_sig_incr;
        *out_samps++ = br_sig_fast;
    }
}

void Math::rnd_aa_a(Math_state *state)
{
    for (int i = 0; i < BL; i = i + 1) {
        *out_samps++ = unifrand_range(x1_samps[i], x2_samps[i]);
    }
}

void Math::rnd_ab_a(Math_state *state)
{
    Sample x2 = *x2_samps;
    for (int i = 0; i < BL; i = i + 1) {
        *out_samps++ = unifrand_range(x1_samps[i], x2);
    }
}

void Math::rnd_ba_a(Math_state *state)
{
    Sample x1 = *x1_samps;
    for (int i = 0; i < BL; i = i + 1) {
        *out_samps++ = unifrand_range(x1, x2_samps[i]);
    }
}

void Math::rnd_bb_a(Math_state *state)
{
    Sample x1 = *x1_samps;
    Sample x2 = *x2_samps;
    for (int i = 0; i < BL; i = i + 1) {
        *out_samps++ = unifrand_range(x1, x2);
    }
}


void Math::sh_aa_a(Math_state *state)
{
    Sample h = state->hold;
    Sample p = state->prev;
    for (int i = 0; i < BL; i = i + 1) {
        Sample x2 = x2_samps[i];
        if (p <= 0 && x2 > 0) {
            h = x1_samps[i];
        }
        p = x2;
        *out_samps++ = h;
    }
    state->prev = p;
    state->hold = h;
}

void Math::sh_ab_a(Math_state *state)
{
    Sample h = state->hold;
    Sample x2 = *x2_samps;
    if (state->prev <= 0 && x2 > 0) {
        h = x1_samps[0];
        state->hold = h;
    }
    state->prev = x2;
    for (int i = 0; i < BL; i = i + 1) {
        *out_samps++ = h;
    }
}

void Math::sh_ba_a(Math_state *state)
{
    Sample h = state->hold;
    Sample p = state->prev;
    Sample x1 = *x1_samps;
    for (int i = 0; i < BL; i = i + 1) {
        Sample x2 = x2_samps[i];
        if (p <= 0 && x2 > 0) {
            h = x1;
        }
        p = x2;
        *out_samps++ = h;
    }
    state->prev = p;
    state->hold = h;
}

void Math::sh_bb_a(Math_state *state)
{
    Sample h = state->hold;
    Sample x1 = *x1_samps;
    Sample x2 = *x2_samps; 
    if (state->prev <= 0 && x2 > 0) {
        h = x1;
        state->hold = h;
    }
    state->prev = x2;
    for (int i = 0; i < BL; i = i + 1) {
        *out_samps++ = h;
    }
}


// let q = x2 * 0x1000, and x2 == 1 means we have quantization levels where -1
// maps to -2^15, +1 maps to 2^15, and 0 maps to 0. That's actually 2^16+1
// total quantization steps. In conversion to 16-bit audio, +1 is regarded as
// outside the range and gets clipped to 2^15-1, the largest 16-bit int.
// quantization: map from range -1 to +1 to the range 0 to q; then round
// to the nearest integer; then map back to range -1 to +1. In this scheme,
// q = 1 implies one quantization step from -1 to +1, so the signal is
// quantized to 2 values, -1 and +1. In principle, +1 represents an
// out-of-range value > 2^0 - 1, just as in 16-bit audio, 2^16 is out of
// range and > 2^16 - 1, but we can still output an process +1 in floating
// point. In fact, this ugen does NOT clip, so inputs outside of (-1, +1)
// can produce outputs outside of (-1, +1).
void Math::qnt_aa_a(Math_state *state)
{
    for (int i = 0; i < BL; i = i + 1) {
        Sample x2 = x2_samps[i];
        Sample q = x2 * 0x8000;  // incorporates a factor of 0.5
        *out_samps++ = x2 <= 0 ? 0 : round((x1_samps[i] + 1) * q) / q - 1;
    }
}

void Math::qnt_ab_a(Math_state *state)
{
    Sample x2 = *x2_samps;
    if (x2 <= 0) {
        block_zero(out_samps);
        out_samps += BL;
        return;
    }
    Sample q = x2 * 0x8000;  // incorporates a factor of 0.5
    Sample qr = 1 / q;
    for (int i = 0; i < BL; i = i + 1) {
        *out_samps++ = round((x1_samps[i] + 1) * q) * qr - 1;
    }
}

void Math::qnt_ba_a(Math_state *state)
{
    Sample br_sig = *x1_samps;
    Sample br_sig_fast = state->prev;
    Sample br_sig_incr = (br_sig - br_sig_fast) * BL_RECIP;
    state->prev = br_sig;
    for (int i = 0; i < BL; i = i + 1) {
        Sample x2 = x2_samps[i];
        Sample q = x2 * 0x8000;  // incorporates a factor of 0.5
        br_sig_fast += br_sig_incr;
        *out_samps++ = x2 <= 0 ? 0 : round((br_sig_fast + 1) * q) / q - 1;
    }
}

void Math::qnt_bb_a(Math_state *state)
{
    Sample x2 = *x2_samps;
    if (x2 <= 0) {
        block_zero(out_samps);
        out_samps += BL;
        return;
    }
    Sample q = x2 * 0x8000;  // incorporates a factor of 0.5
    Sample qr = 1 / q;

    Sample br_sig = *x1_samps;
    Sample br_sig_fast = state->prev;
    Sample br_sig_incr = (br_sig - br_sig_fast) * BL_RECIP;
    state->prev = br_sig;
    for (int i = 0; i < BL; i = i + 1) {
        br_sig_fast += br_sig_incr;
        *out_samps++ = round((br_sig_fast + 1) * q) * qr - 1;
    }
}


/* RLI = random linear interpolation: linearly interpolate from one
   random point to the next. The time to the next breakpoint is 1/x1
   and the range of values is -x2 to +x2.
   State required is the number of samples remaining to compute before
   the next breakpoint (state->count), the last value output (state->prev)
   and the increment per output sample (state->hold).
 */
void Math::rli_aa_a(Math_state *state)
{
    for (int i = 0; i < BL; i = i + 1) {
        if (state->count == 0) {
            state->count = (int) (AR / x1_samps[i]);
            if (state->count == 0) {  // avoid divide by zero
                state->count = 1;
            }
            float x2 = x2_samps[i];
            float target = unifrand_range(-x2, x2);
            state->hold = (target - state->prev) / state->count; // increment
        }
        state->count--;
        *out_samps++ = (state->prev += state->hold);
    }
}

void Math::rli_ab_a(Math_state *state)
{
    for (int i = 0; i < BL; i = i + 1) {
        if (state->count == 0) {
            state->count = (int) (AR / x1_samps[i]);
            if (state->count == 0) {  // avoid divide by zero
                state->count = 1;
            }
            float target = unifrand_range(-*x2_samps, *x2_samps);
            state->hold = (target - state->prev) / state->count; // increment
        }
        state->count--;
        *out_samps++ = (state->prev += state->hold);
    }
}

void Math::rli_ba_a(Math_state *state)
{
    for (int i = 0; i < BL; i = i + 1) {
        if (state->count == 0) {
            state->count = (int) (AR / *x1_samps);
            if (state->count == 0) {  // avoid divide by zero
                state->count = 1;
            }
            float x2 = x2_samps[i];
            float target = unifrand_range(-x2, x2);
            state->hold = (target - state->prev) / state->count; // increment
        }
        state->count--;
        *out_samps++ = (state->prev += state->hold);
    }
}

void Math::rli_bb_a(Math_state *state)
{
    for (int i = 0; i < BL; i = i + 1) {
        if (state->count == 0) {
            state->count = (int) (AR / *x1_samps);
            if (state->count == 0) {  // avoid divide by zero
                state->count = 1;
            }
            float target = unifrand_range(-*x2_samps, *x2_samps);
            state->hold = (target - state->prev) / state->count; // increment
        }
        state->count--;
        *out_samps++ = (state->prev += state->hold);
    }
}


void (Math::*run_aa_a[NUM_MATH_OPS])(Math_state *state) = {
    &Math::mul_aa_a, &Math::add_aa_a, &Math::sub_aa_a, &Math::div_aa_a,
    &Math::max_aa_a, &Math::min_aa_a, &Math::clp_aa_a, &Math::pow_aa_a,
    &Math::lt_aa_a, &Math::gt_aa_a, &Math::scp_aa_a,  &Math::pwi_aa_a,
    &Math::rnd_aa_a, &Math::sh_aa_a, &Math::qnt_aa_a, &Math::rli_aa_a };
void (Math::*run_ab_a[NUM_MATH_OPS])(Math_state *state) = {
    &Math::mul_ab_a, &Math::add_ab_a, &Math::sub_ab_a, &Math::div_ab_a,
    &Math::max_ab_a, &Math::min_ab_a, &Math::clp_ab_a, &Math::pow_ab_a,
    &Math::lt_ab_a, &Math::gt_ab_a, &Math::scp_ab_a, &Math::pwi_ab_a,
    &Math::rnd_ab_a, &Math::sh_ab_a, &Math::qnt_ab_a, &Math::rli_ab_a };
void (Math::*run_ba_a[NUM_MATH_OPS])(Math_state *state) = {
    &Math::mul_ba_a, &Math::add_ba_a, &Math::sub_ba_a, &Math::div_ba_a,
    &Math::max_ba_a, &Math::min_ba_a, &Math::clp_ba_a, &Math::pow_ba_a,
    &Math::lt_ba_a, &Math::gt_ba_a, &Math::scp_ba_a, &Math::pwi_ba_a,
    &Math::rnd_ba_a, &Math::sh_ba_a, &Math::qnt_ba_a, &Math::rli_ba_a };
void (Math::*run_bb_a[NUM_MATH_OPS])(Math_state *state) = {
    &Math::mul_bb_a, &Math::add_bb_a, &Math::sub_bb_a, &Math::div_bb_a,
    &Math::max_bb_a, &Math::min_bb_a, &Math::clp_bb_a, &Math::pow_bb_a,
    &Math::lt_bb_a, &Math::gt_bb_a, &Math::scp_bb_a, &Math::pwi_bb_a,
    &Math::rnd_bb_a, &Math::sh_bb_a, &Math::qnt_bb_a, &Math::rli_bb_a };


/* O2SM INTERFACE: /arco/math/new int32 id, int32 chans, 
                                  int32 op, int32 x1, int32 x2;
 */
void arco_math_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chans = argv[1]->i;
    int32_t op = argv[2]->i;
    int32_t x1 = argv[3]->i;
    int32_t x2 = argv[4]->i;
    // end unpack message

    ANY_UGEN_FROM_ID(x1_ugen,x1, "arco_math_new");
    ANY_UGEN_FROM_ID(x2_ugen,x2, "arco_math_new");

    new Math(id, chans, op, x1_ugen, x2_ugen);
}


/* O2SM INTERFACE: /arco/math/repl_x1 int32 id, int32 x1_id;
 */
static void arco_math_repl_x1(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t x1_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Math, math, id, "arco_math_repl_x1");
    ANY_UGEN_FROM_ID(x1, x1_id, "arco_math_repl_x1");
    math->repl_x1(x1);
}


/* O2SM INTERFACE: /arco/math/set_x1 int32 id, int32 chan, float val;
 */
static void arco_math_set_x1(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chan = argv[1]->i;
    float val = argv[2]->f;
    // end unpack message

    UGEN_FROM_ID(Math, math, id, "arco_math_set_x1");
    math->set_x1(chan, val);
}


/* O2SM INTERFACE: /arco/math/repl_x2 int32 id, int32 x2_id;
 */
static void arco_math_repl_x2(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t x2_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Math, math, id, "arco_math_repl_x2");
    ANY_UGEN_FROM_ID(x2, x2_id, "arco_math_repl_x2");
    math->repl_x2(x2);
}


/* O2SM INTERFACE: /arco/math/set_x2 int32 id, int32 chan, float val;
 */
static void arco_math_set_x2(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chan = argv[1]->i;
    float val = argv[2]->f;
    // end unpack message

    UGEN_FROM_ID(Math, math, id, "arco_math_set_x2");
    math->set_x2(chan, val);
}


static void math_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/math/new", "iiiii", arco_math_new, NULL, true,
                    true);
    o2sm_method_new("/arco/math/repl_x1", "ii", arco_math_repl_x1, NULL,
                    true, true);
    o2sm_method_new("/arco/math/set_x1", "iif", arco_math_set_x1, NULL,
                    true, true);
    o2sm_method_new("/arco/math/repl_x2", "ii", arco_math_repl_x2, NULL,
                    true, true);
    o2sm_method_new("/arco/math/set_x2", "iif", arco_math_set_x2, NULL,
                    true, true);
    // END INTERFACE INITIALIZATION

    // class initialization code from faust:
}

Initializer math_init_obj(math_init);
