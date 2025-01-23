/* ugenid.h -- Unit Generator ID include file; also constains other shared Ugen constants
 *
 * Roger B. Dannenberg
 * Apr 2023
 */

/* This file is separate from ugen.h so that clients and server can share a
   single definition of the number of Ugen Id's available, some special reserved
   IDs and other constants needed by processes that message the audio thread to
   create and control Ugens.
 */

const int UGEN_TABLE_SIZE = 5000;

// special Unit generator IDs:
const int ZERO_ID = 0;
const int ZEROB_ID = 1;
const int INPUT_ID = 2;
const int OUTPUT_ID = 3;
const int UGEN_BASE_ID = 4;

const int ACTION_TERM = 1;    // if source is terminated, bit 0 is set
const int ACTION_ERROR = 2;
const int ACTION_EXCEPT = 4;
const int ACTION_EVENT = 8;  // normal event but not terminated
const int ACTION_END = 16; // final event such as reaching breakpoint == 0
const int ACTION_REM = 32;   // uid was removed from mix or sum
const int ACTION_FREE = 64;  // sent when ugen is deleted

// fade-in and fade-out types, used in Mix and Fader
const int FADE_LINEAR = 0;        // linear fade
const int FADE_EXPONENTIAL = 1;   // exponential (linear in dB) fade
const int FADE_LOWPASS = 2;       // 1st-order low-pass smoothed fade
const int FADE_SMOOTH = 3;        // raised-cosine (S-curve) fade

const int BLEND_LINEAR = 0;
const int BLEND_POWER = 1;
const int BLEND_45 = 2;

// used by math and mathb:
const int MATH_OP_MUL = 0;
const int MATH_OP_ADD = 1;
const int MATH_OP_SUB = 2;
const int MATH_OP_DIV = 3;
const int MATH_OP_MAX = 4;
const int MATH_OP_MIN = 5;
const int MATH_OP_CLP = 6;  // min(max(x, -y), y) i.e. clip if |x| > y
const int MATH_OP_POW = 7;
const int MATH_OP_LT = 8;
const int MATH_OP_GT = 9;
const int MATH_OP_SCP = 10;
const int MATH_OP_PWI = 11;
const int MATH_OP_RND = 12;
const int MATH_OP_SH = 13;
const int MATH_OP_QNT = 14;
const int MATH_OP_RLI = 15;
const int MATH_OP_HZDIFF = 16;
const int MATH_OP_TAN = 17;
const int MATH_OP_ATAN2 = 18;
const int MATH_OP_SIN = 19;
const int MATH_OP_COS = 20;

#define NUM_MATH_OPS 21

const int UNARY_OP_ABS = 0;
const int UNARY_OP_NEG = 1;
const int UNARY_OP_EXP = 2;
const int UNARY_OP_LOG = 3;
const int UNARY_OP_LOG10 = 4;
const int UNARY_OP_LOG2 = 5;
const int UNARY_OP_SQRT = 6;
const int UNARY_OP_STEP_TO_HZ = 7;
const int UNARY_OP_HZ_TO_STEP = 8;
const int UNARY_OP_VEL_TO_LINEAR = 9;
const int UNARY_OP_LINEAR_TO_VEL = 10;
const int UNARY_OP_DB_TO_LINEAR = 11;
const int UNARY_OP_LINEAR_TO_DB = 12;

#define NUM_UNARY_OPS 13



