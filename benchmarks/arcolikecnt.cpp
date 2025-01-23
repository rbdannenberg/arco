// arcolike.cpp - based on Dannenberg and Thompson, CMJ, implements
//     unit generators in the style of Arco
//
// Roger B. Dannenberg
// Jan 2025

// frequency PWL
/* pwlb -- unit generator for arco
 *
 * Roger B. Dannenberg
 * Jan 2022
 */

#define SIMSECS 3600

#include <climits>
#include "o2internal.h"

/* arcotypes.h -- audio dsp process for Arco
 *
 * Roger B. Dannenberg
 * Dec 2021
 */

typedef float Sample;
typedef Sample *Sample_ptr;

const int ARCO_STRINGMAX = 128;
const double AR = 44100.0;
const double AP = 1.0 / AR;
const int LOG2_BL = 5;
const int BL = 1 << LOG2_BL;  // = 32
const float BL_RECIP = 1.0F / BL;
const double BR = AR / BL;
const double BP = BL / AR;
const int BLOCK_BYTES = BL * sizeof(Sample);

// Note: MAX_BLOCK_COUNT is roughtly INT_MAX for 64-bit integers =
// 9,223,372,036,854,775,807, but if you convert this value to a
// double, rounding error converts it to 9,223,372,036,854,775,808,
// which is INT_MAX + 1 (!). Even this is not consistent -- maybe it
// depends on compiler and rounding modes. That can lead to strange
// errors, when all we really need for MAX_BLOCK_COUNT is a number
// that's greater than any block count you ever expect to
// see. MAX_BLOCK_COUNT is therefore set to the empirically derived
// 0x7fffff8000000000, which seems to be the largest value for which x
// == long(double(x)) == long(float(x)), which should be safer for
// conversions, and which is still a VERY big number.  For 32-bit
// ints, we use a correspondingly smaller number. This is still big
// enough for 16 days of audio at 48kHz sample rate and 32-sample
// blocksize.
#if (INT_MAX == 0x7fffffffffffffff)
const int MAX_BLOCK_COUNT = 0x7fffff8000000000;
#else
#if (INT_MAX == 0x7fffffff)
const int MAX_BLOCK_COUNT = 0x7fffff80;
#endif    
#endif    

const double PI = 3.141592653589793;
const double PI2 = PI * 2;

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

// *********** arco compatibility *************

#define arco_print printf
#define arco_warn printf
#define arco_error printf
void fail()
{
    printf("FATAL ERROR!\n");
    exit(1);
}


#include <cmath>
#include "ugencnt.h"

long real_run_count = 0;

/* BENCHMARKING: NO ARCO_TERM:
void arco_term(O2SM_HANDLER_ARGS);  // normally these handlers are local
// to the compilation unit (.cpp file), but this one is defined in
// ugen.cpp but registered with o2sm_method_new() in audioio.cpp.
*/

/* ugen.cpp -- Unit Generator
 *
 * Roger B. Dannenberg
 * Dec 2021
 */

//BENCHMARKING #include "arcougen.h"
#include "const.h"
//BENCHMARKING #include "audioio.h"
const char *Const_name = "Const";

Vec<Ugen_ptr> ugen_table;
int ugen_table_free_list = 1;
char control_service_addr[64] = "";
int control_service_addr_len = 0;

// note table really goes from 0 to 1 over index range 2 to 102
// there are 2 extra samples at either end to allow for interpolation and
// off-by-one errors
float raised_cosine[COS_TABLE_SIZE + 5] = { 
    0, 0, 0, 0.00024672, 0.000986636, 0.00221902,
    0.00394265, 0.00615583, 0.00885637, 0.0120416, 0.0157084,
    0.0198532, 0.0244717, 0.0295596, 0.0351118, 0.0411227, 0.0475865,
    0.0544967, 0.0618467, 0.069629, 0.077836, 0.0864597, 0.0954915,
    0.104922, 0.114743, 0.124944, 0.135516, 0.146447, 0.157726,
    0.169344, 0.181288, 0.193546, 0.206107, 0.218958, 0.232087,
    0.245479, 0.259123, 0.273005, 0.28711, 0.301426, 0.315938,
    0.330631, 0.345492, 0.360504, 0.375655, 0.390928, 0.406309,
    0.421783, 0.437333, 0.452946, 0.468605, 0.484295, 0.5, 0.515705,
    0.531395, 0.547054, 0.562667, 0.578217, 0.593691, 0.609072,
    0.624345, 0.639496, 0.654508, 0.669369, 0.684062, 0.698574,
    0.71289, 0.726995, 0.740877, 0.754521, 0.767913, 0.781042,
    0.793893, 0.806454, 0.818712, 0.830656, 0.842274, 0.853553,
    0.864484, 0.875056, 0.885257, 0.895078, 0.904508, 0.91354,
    0.922164, 0.930371, 0.938153, 0.945503, 0.952414, 0.958877,
    0.964888, 0.97044, 0.975528, 0.980147, 0.984292, 0.987958,
    0.991144, 0.993844, 0.996057, 0.997781, 0.999013, 0.999753, 1, 1, 1};


const char *OP_TO_STRING[NUM_MATH_OPS] = {
    "MATH_OP_MUL", "MATH_OP_ADD", "MATH_OP_SUB", "MATH_OP_DIV", "MATH_OP_MAX",
    "MATH_OP_MIN", "MATH_OP_CLP", "MATH_OP_POW", "MATH_OP_LT", "MATH_OP_GT",
    "MATH_OP_SCP", "MATH_OP_POWI", "MATH_OP_RND", "MATH_OP_SH", "MATH_OP_QNT",
    "MATH_OP_RLI", "MATH_OP_HZDIFF", "MATH_OP_TAN", "MATH_OP_ATAN" };


const char *UNARY_OP_TO_STRING[NUM_UNARY_OPS] = {
    "UNARY_OP_ABS", "UNARY_OP_NEG", "UNARY_OP_EXP",
    "UNARY_OP_LOG", "UNARY_OP_LOG10", "UNARY_OP_LOG2", "UNARY_OP_SQRT",
    "UNARY_OP_STEP_TO_HZ", "UNARY_OP_HZ_TO_STEP", "UNARY_OP_VEL_TO_LINEAR",
    "UNARY_OP_LINEAR_TO_VEL", "UNARY_OP_DB_TO_LINEAR",
    "UNARY_OP_LINEAR_TO_DB" };


Ugen::~Ugen() {
    // send_action_id(ACTION_FREE);
    // special case: check for run set or output set references
    assert((flags & IN_RUN_SET) == 0);
    // printf("Ugen delete %d\n", id);
}


void Ugen::unref() {
    refcount--;
    // printf("Ugen::unref id %d %s new refcount %d\n",
    //        id, classname(), refcount);
    if (refcount == 0) {
        on_terminate(ACTION_FREE);  // notify `atend` mechanism if it is
        // pending this should be in destructor, but destructors cannot
        // call inherited methods in c++.
        if (flags & UGENTRACE) {
            arco_print("Deleting traced ugen: "); print();
        }
        delete this;
    }
}


void Ugen::indent_spaces(int indent)
{
    for (int i = 0; i < indent; i++) {
        arco_print("  ");
    }
}


// Both param ("") and revisiting (false) are optional
void Ugen::print(int indent, const char *param, bool revisiting) {
    const char *openp = "(";
    const char *closep = ") ";
    if (!*param) {
        openp = closep = "";
    }
    const char *was = (this == ugen_table[id] ? "" : "(was) ");
    arco_print("%s %s%s%sid %s%d refs %d chans %d ",
               classname(), openp, param, closep, was, id, refcount, chans);
    if (flags & TERMINATED) {
        arco_print("TERMINATED ");
    } else if (flags & TERMINATING) {
        arco_print("terminate in %g sec ", tail_blocks * BP);
    } else if (flags & CAN_TERMINATE) {
        arco_print("can terminate ");
    }
    print_details(indent);
    if (revisiting) {
        arco_print(" (shown above)");
    }
    arco_print("\n");
}    


// this method runs a mark and print algorithm when print is true
// it simply unmarks everything when print is false
//
void Ugen::print_tree(int indent, bool print_flag, const char *param)
{
    bool visited = ((flags & UGEN_MARK) != 0);
    if (print_flag) {
        indent_spaces(indent);
        print(indent, param, visited);
        flags |= UGEN_MARK;
        if (visited) {
            return;
        }
    } else {
        if (!visited) {
            return;  // cut off the search; we've been here before
        }
        flags &= ~UGEN_MARK;  // clear flag
    }
    // tricky logic: if we're printing and haven't visited this ugen
    // yet then we print the sub-tree; otherwise, if we're not printing
    // and just clearing all the flags, we get here if the flag is set
    // (visited == true), so we visit the subtree to clear subtree flags.
    // In both cases, if we are here, then print_flag == !visited.
    print_sources(indent + 1, print_flag);
}


void Ugen::const_set(int chan, Sample x, const char *from)
// Assume this is a Const. Set channel chan of the const output to x.
// Do nothing but print warnings if this is not a Const ugen.
// Normally, this is called to implement a "set_input" or in general
// "set_xxxx" where xxxx is a signal input to another ugen, so from
// is a string name of the method, e.g. "Upsample::set_input".
{
    if (rate != 'c') {
        arco_warn("%s: ugen (%d) is not a Const\n", from, id);
    } else {
        ((Const *) this)->set_value(chan, x, from);
    }
}


void ugen_initialize()
{
    ugen_table.init(UGEN_TABLE_SIZE, true);  // fill with zero, exact size
}


// Look up the Ugen associated with id: if id is out of bounds or
// there is no ugen, or the ugen has an unexpected class, return NULL.
// If the class of the Ugen does not matter, pass NULL for classname.
Ugen_ptr id_to_ugen(int32_t id, const char *classname, const char *operation)
{
    if (!ugen_table.bounds_check(id)) {
        arco_error("Bad ugen index %d in %s\n", id, operation);
        return NULL;
    }
    Ugen_ptr ugen = ugen_table[id];
    if (!ugen) {
        arco_error("%s uninitialized id %d, ignored\n", operation, id);
    } else if (classname && (ugen->classname() != classname)) {
        arco_error("%s id %d found a %s Ugen instead of %s, ignored\n",
                   operation, id, ugen->classname(), classname);
        ugen = NULL;
    }
    return ugen;
}


// float vector x += y
void block_add_n(Sample_ptr x, Sample_ptr y, int n)
{
    n *= BL;
    for (int i = 0; i < n; i++) {
        *x++ += *y++;
    }
}



// *********** pwlb *************

const char *Pwlb_name = "Pwlb";

class Pwlb : public Ugen {
public:
    float current;   // value of the next output sample
    int seg_togo;    // remaining samples in current segment
    float seg_incr;  // increment from one sample to the next in current segment
    float final_value; // target value that will be first sample of next segment
    int next_point_index;  // index into point of the next segment
    int action_id;         // send this when envelope is done
    Vec<float> points;     // envelope breakpoints (seg_len, final_value)

    Pwlb(int id) : Ugen(id, 'b', 1) {
        current = 0.0f;        // real_run() will compute the first envelope
        seg_togo = INT_MAX;    //     after start(); initial output is 0
        seg_incr = 0.0;
        final_value = 0.0f;    // if no envelope is loaded, output will
        next_point_index = 0;  //     become constant zero.
        action_id = 0;
        points.init(0);        // initially empty, so size = 0
    }

    const char *classname() { return Pwlb_name; }

    void real_run() {
        // using while allows durations of zero so that we can have an
        // initial non-zero value by setting initial duration to 0:
        while (seg_togo == 0) { // set up next segment
            current = final_value;  // make output value exact
            if (next_point_index >= points.size()) {
                seg_incr = 0.0f;
                // if we can terminate, send_action_id when termination
                // is complete (done by terminate(ACTION_TERM)):
                if (current == 0 && (flags & CAN_TERMINATE)) {
                    // keep polling terminate until it happens:
                    if (terminate(ACTION_EVENT | ACTION_END)) {
                        seg_togo = INT_MAX;  // then this stops polling until start()
                    }
                } else {  // otherwise, just send_action_id now:
                    int status = ACTION_EVENT;
                    if (final_value == 0) {
                        status |= ACTION_END;
                    }
                    send_action_id(status);
                }
                break;
            } else {
                seg_togo = (int) points[next_point_index++];
                final_value = points[next_point_index++];
                seg_incr = (final_value - current) / seg_togo;
            }
        }
        *out_samps = current;
        current += seg_incr;
        seg_togo--;
    }
    

    void start() {
        next_point_index = 0;
        seg_togo = 0;
        final_value = current;  // continue from current, whatever it is
    }


    void stop() {
        seg_togo = INT_MAX;
        seg_incr = 0.0f;
    }


    void decay(float d) {
        seg_togo = (int) d;
        seg_incr = -current / seg_togo;
        next_point_index = points.size();  // end of envelope
        final_value = 0.0f;
    }


    void set(float y) {
        current = y;
    }
    

    void point(float f) { points.push_back(f); }
};

typedef Pwlb *Pwlb_ptr;


/* wavetables.cpp -- abstract superclass for unit generators with wavetables
 *
 * Roger B. Dannenberg
 * Oct 2024
 */

const int sine_table_len = 1024;

float sine_table[1025] = {
    0, 0.00613588, 0.0122715, 0.0184067, 0.0245412, 
    0.0306748, 0.0368072, 0.0429383, 0.0490677, 0.0551952, 
    0.0613207, 0.0674439, 0.0735646, 0.0796824, 0.0857973, 
    0.091909, 0.0980171, 0.104122, 0.110222, 0.116319, 
    0.122411, 0.128498, 0.134581, 0.140658, 0.14673, 
    0.152797, 0.158858, 0.164913, 0.170962, 0.177004, 
    0.18304, 0.189069, 0.19509, 0.201105, 0.207111, 
    0.21311, 0.219101, 0.225084, 0.231058, 0.237024, 
    0.24298, 0.248928, 0.254866, 0.260794, 0.266713, 
    0.272621, 0.27852, 0.284408, 0.290285, 0.296151, 
    0.302006, 0.30785, 0.313682, 0.319502, 0.32531, 
    0.331106, 0.33689, 0.342661, 0.348419, 0.354164, 
    0.359895, 0.365613, 0.371317, 0.377007, 0.382683, 
    0.388345, 0.393992, 0.399624, 0.405241, 0.410843, 
    0.41643, 0.422, 0.427555, 0.433094, 0.438616, 
    0.444122, 0.449611, 0.455084, 0.460539, 0.465976, 
    0.471397, 0.476799, 0.482184, 0.48755, 0.492898, 
    0.498228, 0.503538, 0.50883, 0.514103, 0.519356, 
    0.52459, 0.529804, 0.534998, 0.540171, 0.545325, 
    0.550458, 0.55557, 0.560662, 0.565732, 0.570781, 
    0.575808, 0.580814, 0.585798, 0.59076, 0.595699, 
    0.600616, 0.605511, 0.610383, 0.615232, 0.620057, 
    0.624859, 0.629638, 0.634393, 0.639124, 0.643832, 
    0.648514, 0.653173, 0.657807, 0.662416, 0.667, 
    0.671559, 0.676093, 0.680601, 0.685084, 0.689541, 
    0.693971, 0.698376, 0.702755, 0.707107, 0.711432, 
    0.715731, 0.720003, 0.724247, 0.728464, 0.732654, 
    0.736817, 0.740951, 0.745058, 0.749136, 0.753187, 
    0.757209, 0.761202, 0.765167, 0.769103, 0.77301, 
    0.776888, 0.780737, 0.784557, 0.788346, 0.792107, 
    0.795837, 0.799537, 0.803208, 0.806848, 0.810457, 
    0.814036, 0.817585, 0.821103, 0.824589, 0.828045, 
    0.83147, 0.834863, 0.838225, 0.841555, 0.844854, 
    0.84812, 0.851355, 0.854558, 0.857729, 0.860867, 
    0.863973, 0.867046, 0.870087, 0.873095, 0.87607, 
    0.879012, 0.881921, 0.884797, 0.88764, 0.890449, 
    0.893224, 0.895966, 0.898674, 0.901349, 0.903989, 
    0.906596, 0.909168, 0.911706, 0.91421, 0.916679, 
    0.919114, 0.921514, 0.92388, 0.92621, 0.928506, 
    0.930767, 0.932993, 0.935184, 0.937339, 0.939459, 
    0.941544, 0.943593, 0.945607, 0.947586, 0.949528, 
    0.951435, 0.953306, 0.955141, 0.95694, 0.958703, 
    0.960431, 0.962121, 0.963776, 0.965394, 0.966976, 
    0.968522, 0.970031, 0.971504, 0.97294, 0.974339, 
    0.975702, 0.977028, 0.978317, 0.97957, 0.980785, 
    0.981964, 0.983105, 0.98421, 0.985278, 0.986308, 
    0.987301, 0.988258, 0.989177, 0.990058, 0.990903, 
    0.99171, 0.99248, 0.993212, 0.993907, 0.994565, 
    0.995185, 0.995767, 0.996313, 0.99682, 0.99729, 
    0.997723, 0.998118, 0.998476, 0.998795, 0.999078, 
    0.999322, 0.999529, 0.999699, 0.999831, 0.999925, 
    0.999981, 1, 0.999981, 0.999925, 0.999831, 
    0.999699, 0.999529, 0.999322, 0.999078, 0.998795, 
    0.998476, 0.998118, 0.997723, 0.99729, 0.99682, 
    0.996313, 0.995767, 0.995185, 0.994565, 0.993907, 
    0.993212, 0.99248, 0.99171, 0.990903, 0.990058, 
    0.989177, 0.988258, 0.987301, 0.986308, 0.985278, 
    0.98421, 0.983105, 0.981964, 0.980785, 0.97957, 
    0.978317, 0.977028, 0.975702, 0.974339, 0.97294, 
    0.971504, 0.970031, 0.968522, 0.966976, 0.965394, 
    0.963776, 0.962121, 0.960431, 0.958703, 0.95694, 
    0.955141, 0.953306, 0.951435, 0.949528, 0.947586, 
    0.945607, 0.943593, 0.941544, 0.939459, 0.937339, 
    0.935184, 0.932993, 0.930767, 0.928506, 0.92621, 
    0.92388, 0.921514, 0.919114, 0.916679, 0.91421, 
    0.911706, 0.909168, 0.906596, 0.903989, 0.901349, 
    0.898674, 0.895966, 0.893224, 0.890449, 0.88764, 
    0.884797, 0.881921, 0.879012, 0.87607, 0.873095, 
    0.870087, 0.867046, 0.863973, 0.860867, 0.857729, 
    0.854558, 0.851355, 0.84812, 0.844854, 0.841555, 
    0.838225, 0.834863, 0.83147, 0.828045, 0.824589, 
    0.821103, 0.817585, 0.814036, 0.810457, 0.806848, 
    0.803208, 0.799537, 0.795837, 0.792107, 0.788346, 
    0.784557, 0.780737, 0.776888, 0.77301, 0.769103, 
    0.765167, 0.761202, 0.757209, 0.753187, 0.749136, 
    0.745058, 0.740951, 0.736817, 0.732654, 0.728464, 
    0.724247, 0.720003, 0.715731, 0.711432, 0.707107, 
    0.702755, 0.698376, 0.693971, 0.689541, 0.685084, 
    0.680601, 0.676093, 0.671559, 0.667, 0.662416, 
    0.657807, 0.653173, 0.648514, 0.643832, 0.639124, 
    0.634393, 0.629638, 0.624859, 0.620057, 0.615232, 
    0.610383, 0.605511, 0.600616, 0.595699, 0.59076, 
    0.585798, 0.580814, 0.575808, 0.570781, 0.565732, 
    0.560662, 0.55557, 0.550458, 0.545325, 0.540171, 
    0.534998, 0.529804, 0.52459, 0.519356, 0.514103, 
    0.50883, 0.503538, 0.498228, 0.492898, 0.48755, 
    0.482184, 0.476799, 0.471397, 0.465976, 0.460539, 
    0.455084, 0.449611, 0.444122, 0.438616, 0.433094, 
    0.427555, 0.422, 0.41643, 0.410843, 0.405241, 
    0.399624, 0.393992, 0.388345, 0.382683, 0.377007, 
    0.371317, 0.365613, 0.359895, 0.354164, 0.348419, 
    0.342661, 0.33689, 0.331106, 0.32531, 0.319502, 
    0.313682, 0.30785, 0.302006, 0.296151, 0.290285, 
    0.284408, 0.27852, 0.272621, 0.266713, 0.260794, 
    0.254866, 0.248928, 0.24298, 0.237024, 0.231058, 
    0.225084, 0.219101, 0.21311, 0.207111, 0.201105, 
    0.19509, 0.189069, 0.18304, 0.177004, 0.170962, 
    0.164913, 0.158858, 0.152797, 0.14673, 0.140658, 
    0.134581, 0.128498, 0.122411, 0.116319, 0.110222, 
    0.104122, 0.0980171, 0.091909, 0.0857973, 0.0796824, 
    0.0735646, 0.0674439, 0.0613207, 0.0551952, 0.0490677, 
    0.0429383, 0.0368072, 0.0306748, 0.0245412, 0.0184067, 
    0.0122715, 0.00613588, 1.22465e-16, -0.00613588, -0.0122715, 
    -0.0184067, -0.0245412, -0.0306748, -0.0368072, -0.0429383, 
    -0.0490677, -0.0551952, -0.0613207, -0.0674439, -0.0735646, 
    -0.0796824, -0.0857973, -0.091909, -0.0980171, -0.104122, 
    -0.110222, -0.116319, -0.122411, -0.128498, -0.134581, 
    -0.140658, -0.14673, -0.152797, -0.158858, -0.164913, 
    -0.170962, -0.177004, -0.18304, -0.189069, -0.19509, 
    -0.201105, -0.207111, -0.21311, -0.219101, -0.225084, 
    -0.231058, -0.237024, -0.24298, -0.248928, -0.254866, 
    -0.260794, -0.266713, -0.272621, -0.27852, -0.284408, 
    -0.290285, -0.296151, -0.302006, -0.30785, -0.313682, 
    -0.319502, -0.32531, -0.331106, -0.33689, -0.342661, 
    -0.348419, -0.354164, -0.359895, -0.365613, -0.371317, 
    -0.377007, -0.382683, -0.388345, -0.393992, -0.399624, 
    -0.405241, -0.410843, -0.41643, -0.422, -0.427555, 
    -0.433094, -0.438616, -0.444122, -0.449611, -0.455084, 
    -0.460539, -0.465976, -0.471397, -0.476799, -0.482184, 
    -0.48755, -0.492898, -0.498228, -0.503538, -0.50883, 
    -0.514103, -0.519356, -0.52459, -0.529804, -0.534998, 
    -0.540171, -0.545325, -0.550458, -0.55557, -0.560662, 
    -0.565732, -0.570781, -0.575808, -0.580814, -0.585798, 
    -0.59076, -0.595699, -0.600616, -0.605511, -0.610383, 
    -0.615232, -0.620057, -0.624859, -0.629638, -0.634393, 
    -0.639124, -0.643832, -0.648514, -0.653173, -0.657807, 
    -0.662416, -0.667, -0.671559, -0.676093, -0.680601, 
    -0.685084, -0.689541, -0.693971, -0.698376, -0.702755, 
    -0.707107, -0.711432, -0.715731, -0.720003, -0.724247, 
    -0.728464, -0.732654, -0.736817, -0.740951, -0.745058, 
    -0.749136, -0.753187, -0.757209, -0.761202, -0.765167, 
    -0.769103, -0.77301, -0.776888, -0.780737, -0.784557, 
    -0.788346, -0.792107, -0.795837, -0.799537, -0.803208, 
    -0.806848, -0.810457, -0.814036, -0.817585, -0.821103, 
    -0.824589, -0.828045, -0.83147, -0.834863, -0.838225, 
    -0.841555, -0.844854, -0.84812, -0.851355, -0.854558, 
    -0.857729, -0.860867, -0.863973, -0.867046, -0.870087, 
    -0.873095, -0.87607, -0.879012, -0.881921, -0.884797, 
    -0.88764, -0.890449, -0.893224, -0.895966, -0.898674, 
    -0.901349, -0.903989, -0.906596, -0.909168, -0.911706, 
    -0.91421, -0.916679, -0.919114, -0.921514, -0.92388, 
    -0.92621, -0.928506, -0.930767, -0.932993, -0.935184, 
    -0.937339, -0.939459, -0.941544, -0.943593, -0.945607, 
    -0.947586, -0.949528, -0.951435, -0.953306, -0.955141, 
    -0.95694, -0.958703, -0.960431, -0.962121, -0.963776, 
    -0.965394, -0.966976, -0.968522, -0.970031, -0.971504, 
    -0.97294, -0.974339, -0.975702, -0.977028, -0.978317, 
    -0.97957, -0.980785, -0.981964, -0.983105, -0.98421, 
    -0.985278, -0.986308, -0.987301, -0.988258, -0.989177, 
    -0.990058, -0.990903, -0.99171, -0.99248, -0.993212, 
    -0.993907, -0.994565, -0.995185, -0.995767, -0.996313, 
    -0.99682, -0.99729, -0.997723, -0.998118, -0.998476, 
    -0.998795, -0.999078, -0.999322, -0.999529, -0.999699, 
    -0.999831, -0.999925, -0.999981, -1, -0.999981, 
    -0.999925, -0.999831, -0.999699, -0.999529, -0.999322, 
    -0.999078, -0.998795, -0.998476, -0.998118, -0.997723, 
    -0.99729, -0.99682, -0.996313, -0.995767, -0.995185, 
    -0.994565, -0.993907, -0.993212, -0.99248, -0.99171, 
    -0.990903, -0.990058, -0.989177, -0.988258, -0.987301, 
    -0.986308, -0.985278, -0.98421, -0.983105, -0.981964, 
    -0.980785, -0.97957, -0.978317, -0.977028, -0.975702, 
    -0.974339, -0.97294, -0.971504, -0.970031, -0.968522, 
    -0.966976, -0.965394, -0.963776, -0.962121, -0.960431, 
    -0.958703, -0.95694, -0.955141, -0.953306, -0.951435, 
    -0.949528, -0.947586, -0.945607, -0.943593, -0.941544, 
    -0.939459, -0.937339, -0.935184, -0.932993, -0.930767, 
    -0.928506, -0.92621, -0.92388, -0.921514, -0.919114, 
    -0.916679, -0.91421, -0.911706, -0.909168, -0.906596, 
    -0.903989, -0.901349, -0.898674, -0.895966, -0.893224, 
    -0.890449, -0.88764, -0.884797, -0.881921, -0.879012, 
    -0.87607, -0.873095, -0.870087, -0.867046, -0.863973, 
    -0.860867, -0.857729, -0.854558, -0.851355, -0.84812, 
    -0.844854, -0.841555, -0.838225, -0.834863, -0.83147, 
    -0.828045, -0.824589, -0.821103, -0.817585, -0.814036, 
    -0.810457, -0.806848, -0.803208, -0.799537, -0.795837, 
    -0.792107, -0.788346, -0.784557, -0.780737, -0.776888, 
    -0.77301, -0.769103, -0.765167, -0.761202, -0.757209, 
    -0.753187, -0.749136, -0.745058, -0.740951, -0.736817, 
    -0.732654, -0.728464, -0.724247, -0.720003, -0.715731, 
    -0.711432, -0.707107, -0.702755, -0.698376, -0.693971, 
    -0.689541, -0.685084, -0.680601, -0.676093, -0.671559, 
    -0.667, -0.662416, -0.657807, -0.653173, -0.648514, 
    -0.643832, -0.639124, -0.634393, -0.629638, -0.624859, 
    -0.620057, -0.615232, -0.610383, -0.605511, -0.600616, 
    -0.595699, -0.59076, -0.585798, -0.580814, -0.575808, 
    -0.570781, -0.565732, -0.560662, -0.55557, -0.550458, 
    -0.545325, -0.540171, -0.534998, -0.529804, -0.52459, 
    -0.519356, -0.514103, -0.50883, -0.503538, -0.498228, 
    -0.492898, -0.48755, -0.482184, -0.476799, -0.471397, 
    -0.465976, -0.460539, -0.455084, -0.449611, -0.444122, 
    -0.438616, -0.433094, -0.427555, -0.422, -0.41643, 
    -0.410843, -0.405241, -0.399624, -0.393992, -0.388345, 
    -0.382683, -0.377007, -0.371317, -0.365613, -0.359895, 
    -0.354164, -0.348419, -0.342661, -0.33689, -0.331106, 
    -0.32531, -0.319502, -0.313682, -0.30785, -0.302006, 
    -0.296151, -0.290285, -0.284408, -0.27852, -0.272621, 
    -0.266713, -0.260794, -0.254866, -0.248928, -0.24298, 
    -0.237024, -0.231058, -0.225084, -0.219101, -0.21311, 
    -0.207111, -0.201105, -0.19509, -0.189069, -0.18304, 
    -0.177004, -0.170962, -0.164913, -0.158858, -0.152797, 
    -0.14673, -0.140658, -0.134581, -0.128498, -0.122411, 
    -0.116319, -0.110222, -0.104122, -0.0980171, -0.091909, 
    -0.0857973, -0.0796824, -0.0735646, -0.0674439, -0.0613207, 
    -0.0551952, -0.0490677, -0.0429383, -0.0368072, -0.0306748, 
    -0.0245412, -0.0184067, -0.0122715, -0.00613588, 0};



/* wavetables -- abstract superclass for unit generators with wavetables
 *
 * Roger B. Dannenberg
 * Oct 2024
 */

/* wavetables of length n represent a period of length n-2 where
 * w[0] repeats at w[n-2] and w[1] repeats at w[n-1] to simplify
 * interpolation.
 */
typedef Vec<float> Wavetable;

class Wavetables : public Ugen {
public:
    Vec<Wavetable> wavetables;
    Wavetables *lender;  // when non-null, wavetables are fetched from lender,
                         // allowing wavetables to be shared by many oscillators

    Wavetables(int id, int nchans) : Ugen(id, 'a', nchans), wavetables(0) {
        lender = nullptr;
    }


    ~Wavetables() {
        for (int i = 0; i < wavetables.size(); i++) {
            wavetables[i].finish();
        }
        if (lender) {
            lender->unref();
        }
    }


    void borrow(Wavetables *wt) {
        lender = wt;
        lender->ref();
    }


    int num_tables() {
        return lender ? lender->wavetables.size() : wavetables.size();
    }


    Wavetable *get_table(int i) {
        if (lender) {
            if (i >= lender->wavetables.size()) {
                if (lender->wavetables.size() > 0) {
                    i = 0;
                } else {
                    return nullptr;
                }
            }
            return &lender->wavetables[0];
        } else {
            if (i >= wavetables.size()) {
                if (wavetables.size() > 0) {
                    i = 0;
                } else {
                    return nullptr;
                }
            }
            return &wavetables[0];
        }
    }


    void create_table_at(int i, int tlen) {
        // if i > size, extend wavetables and initialize to empty wavetables
        int n = wavetables.size();
        if (n <= i) {
            wavetables.set_size(i + 1, false);
            for (; n < i + 1; n++) {
                wavetables[i].init(0);
            }
        }
        wavetables[i].set_size(tlen + 2, false);
    }

    double schroeder_phase(int n, int slen) {
        // here, n is zero-based, so use (n + 1) * n rather than n * (n - 1):
        return M_PI * (n + 1) * n / slen;
    }

    // Uses Schroeder's formula for phase, intended to reduce crest factor
    // by avoiding all in-phase harmonics: φ(n) = π * n * (n - 1) / N
    void create_table(int i, int tlen, int slen, float *spec, bool hasphase) {
        create_table_at(i, tlen);
        // now wavetables[i] exists and is initialized but may be wrong size
        Wavetable &table = wavetables[i];
        table.zero();
        int harm = 1;  // harmonic number
        for (int h = 0; h < slen - hasphase; h += 1 + hasphase) {
            float *data = &table[0];
            float amp = spec[h];
            double phase = (hasphase ? spec[h + 1] : schroeder_phase(h, slen));
            double phase_incr = (double) harm * sine_table_len / tlen;
            for (i = 0; i < tlen + 2; i++) {
                int iphase = phase;
                float phase_frac = phase - iphase;
                *data++ += (sine_table[iphase] * (1 - phase_frac) +
                            sine_table[iphase + 1] * phase_frac) * amp;
                phase += phase_incr;
                if (phase >= sine_table_len) {
                    phase -= sine_table_len;
                }
            }
            harm++;
        }
    }


    // set ith table of length tlen from an amplitude spectrum of length alen
    // tlen is the "period" of the table and 
    // the actual allocation size is tlen + 2
    void create_tas(int i, int tlen, int slen, float *ampspec) {
        create_table(i, tlen, slen, ampspec, false);
    }


    // set ith table of length tlen from a complex spectrum (amplitude and
    // phase) of length alen. Phase is in *radians*.
    void create_tcs(int i, int tlen, int slen, float *spec) {
        create_table(i, tlen, slen, spec, true);
    }


    // set ith table of length tlen from time domain data
    void create_ttd(int i, int tlen, float *samps) {
        create_table_at(i, tlen);
        // now wavetables[i] exists and is initialized but may be wrong size
        Wavetable &table = wavetables[i];
        float *data = &table[0];
        memcpy(data, samps, tlen * sizeof(float));
        table[tlen] = samps[0];
        table[tlen + 1] = samps[1];
    }
};



// ********** tableosc *************
/* tableosc.h -- simple table-lookup oscillator
 *
 * Roger B. Dannenberg
 * Oct 2024
 */

const char *Tableosc_name = "Tableosc";

class Tableosc : public Wavetables {
public:
    struct Tableosc_state {
        double phase;  // from 0 to 1 (1 represents 2PI or 360 degrees)
        Sample prev_amp;
    };
    int which_table;
    Vec<Tableosc_state> states;
    void (Tableosc::*run_channel)(Tableosc_state *state,
                                  Sample *table, int tlen);


    Ugen_ptr freq;
    int freq_stride;
    Sample_ptr freq_samps;

    Ugen_ptr amp;
    int amp_stride;
    Sample_ptr amp_samps;


    Tableosc(int id, int nchans, Ugen_ptr freq_, Ugen_ptr amp_, float phase) :
            Wavetables(id, nchans) {
        which_table = 0;
        states.set_size(chans);
        for (int i = 0; i < chans; i++) {
            states[i].phase = fmodf(phase / 360.0f, 1.0f);
        }
        init_freq(freq_);
        init_amp(amp_);
        update_run_channel();
    }

    ~Tableosc() {
        freq->unref();
    }

    const char *classname() { return Tableosc_name; }

    void update_run_channel() {
        if (freq->rate == 'a') {
            if (amp->rate == 'a') {
                run_channel = &Tableosc::chan_aa_a;
            } else {
                run_channel = &Tableosc::chan_ab_a;
            }
        } else {
            if (amp->rate == 'a') {
                run_channel = &Tableosc::chan_ba_a;
            } else {
                run_channel = &Tableosc::chan_bb_a;
            }
        }
    }

    void print_sources(int indent, bool print_flag) {
        freq->print_tree(indent, print_flag, "freq");
        amp->print_tree(indent, print_flag, "amp");
    }

    void repl_freq(Ugen_ptr ugen) {
        freq->unref();
        init_freq(ugen);
        update_run_channel();
    }

    void set_freq(int chan, float f) {
        freq->const_set(chan, f, "Tableosc::set_freq");
    }

    void init_freq(Ugen_ptr ugen) { init_param(ugen, freq, &freq_stride); }

    void repl_amp(Ugen_ptr ugen) {
        amp->unref();
        init_amp(ugen);
        update_run_channel();
    }

    void set_amp(int chan, float f) {
        amp->const_set(chan, f, "Tableosc::set_amp");
    }

    void init_amp(Ugen_ptr ugen) { init_param(ugen, amp, &amp_stride); }


    void select(int i) {  // select table
        if (i < 0 || i >= num_tables()) {
            i = 0;
        }
        which_table = i;
        Wavetable *table = get_table(i);
    }


    void chan_aa_a(Tableosc_state *state, Sample *table, int tlen) {
        double phase = state->phase;
        for (int i = 0; i < BL; i++) {
            float x = phase * tlen;
            int ix = x;
            float frac = x - ix;
            *out_samps++ = (table[ix] * (1 - frac) + 
                            table[ix + 1] * frac) * amp_samps[i];
            phase += freq_samps[i] * AP;
            while (phase > 1) phase--;
            while (phase < 0) phase++;
        }
        state->phase = phase;
    }


    void chan_ba_a(Tableosc_state *state, Sample *table, int tlen) {
        double phase = state->phase;
        double phase_incr = *freq_samps * AP;
        for (int i = 0; i < BL; i++) {
            float x = phase * tlen;
            int ix = x;
            float frac = x - ix;
            *out_samps++ = (table[ix] * (1 - frac) + 
                            table[ix + 1] * frac) * amp_samps[i];
            phase += phase_incr;
            while (phase > 1) phase--;
            while (phase < 0) phase++;
        }
        state->phase = phase;
    }


    void chan_ab_a(Tableosc_state *state, Sample *table, int tlen) {
        double phase = state->phase;
        Sample amp_sig = *amp_samps;
        Sample amp_sig_fast = state->prev_amp;
        state->prev_amp = amp_sig;
        Sample amp_sig_incr = (amp_sig - amp_sig_fast) * BL_RECIP;
        for (int i = 0; i < BL; i++) {
            float x = phase * tlen;
            int ix = x;
            float frac = x - ix;
            amp_sig_fast += amp_sig_incr;
            *out_samps++ = (table[ix] * (1 - frac) + 
                            table[ix + 1] * frac) * amp_sig_fast;
            phase += freq_samps[i] * AP;
            while (phase > 1) phase--;
            while (phase < 0) phase++;
        }
        state->phase = phase;
    }


    void chan_bb_a(Tableosc_state *state, Sample *table, int tlen) {
        double phase = state->phase;
        double phase_incr = *freq_samps * AP;
        Sample amp_sig = *amp_samps;
        Sample amp_sig_fast = state->prev_amp;
        state->prev_amp = amp_sig;
        Sample amp_sig_incr = (amp_sig - amp_sig_fast) * BL_RECIP;
        for (int i = 0; i < BL; i++) {
            float x = phase * tlen;
            int ix = x;
            float frac = x - ix;
            amp_sig_fast += amp_sig_incr;
            *out_samps++ = (table[ix] * (1 - frac) + 
                            table[ix + 1] * frac) * amp_sig_fast;
            phase += phase_incr;
            while (phase > 1) phase--;
            while (phase < 0) phase++;
        }
        state->phase = phase;
    }


    void real_run() {
        freq_samps = freq->run(current_block); // update input
        amp_samps = amp->run(current_block); // update input
        Tableosc_state *state = &states[0];
        if (which_table >= num_tables()) {
            return;
        }
        Wavetable *table = get_table(which_table);
        if (!table) {
            return;
        }
        int tlen = table->size() - 2;
        if (tlen < 2) {
            return;
        }
        for (int i = 0; i < chans; i++) {
            (this->*run_channel)(state, &((*table)[0]), tlen);
            state++;
            freq_samps += freq_stride;
            amp_samps += amp_stride;
        }
    }
};

typedef Tableosc *Tableosc_ptr;

// *********** sineb **************
/* sineb -- unit generator for arco
 *
 * generated by f2a.py
 */

/*------------- BEGIN FAUST PREAMBLE -------------*/

/* ------------------------------------------------------------
name: "sineb"
Code generated with Faust 2.75.7 (https://faust.grame.fr)
Compilation options: -lang cpp -os -light -ct 1 -cn Sineb -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 0
------------------------------------------------------------ */

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <math.h>

#ifndef FAUSTCLASS 
#define FAUSTCLASS Sineb
#endif

#ifdef __APPLE__ 
#define exp10f __exp10f
#define exp10 __exp10
#endif

#if defined(_WIN32)
#define RESTRICT __restrict
#else
#define RESTRICT __restrict__
#endif

class SinebSIG0 {
    
  private:
    
    int iVec0[2];
    int iRec0[2];
    
  public:
    
    int getNumInputsSinebSIG0() {
        return 0;
    }
    int getNumOutputsSinebSIG0() {
        return 1;
    }
    
    void instanceInitSinebSIG0(int sample_rate) {
        for (int l0 = 0; l0 < 2; l0 = l0 + 1) {
            iVec0[l0] = 0;
        }
        for (int l1 = 0; l1 < 2; l1 = l1 + 1) {
            iRec0[l1] = 0;
        }
    }
    
    void fillSinebSIG0(int count, float* table) {
        for (int i1 = 0; i1 < count; i1 = i1 + 1) {
            iVec0[0] = 1;
            iRec0[0] = (iVec0[1] + iRec0[1]) % 65536;
            table[i1] = std::sin(9.58738e-05f * float(iRec0[0]));
            iVec0[1] = iVec0[0];
            iRec0[1] = iRec0[0];
        }
    }

};

static SinebSIG0* newSinebSIG0() { return (SinebSIG0*)new SinebSIG0(); }
static void deleteSinebSIG0(SinebSIG0* dsp) { delete dsp; }

static float ftbl0SinebSIG0[65536];
/*-------------- END FAUST PREAMBLE --------------*/

const char *Sineb_name = "Sineb";

class Sineb : public Ugen {
public:
    struct Sineb_state {
        FAUSTFLOAT fEntry0;
        int iVec1[2];
        FAUSTFLOAT fEntry1;
        float fRec1[2];
    };
    Vec<Sineb_state> states;
    void (Sineb::*run_channel)(Sineb_state *state);

    Ugen_ptr freq;
    int freq_stride;
    Sample_ptr freq_samps;

    Ugen_ptr amp;
    int amp_stride;
    Sample_ptr amp_samps;

    float fConst0;

    Sineb(int id, int nchans, Ugen_ptr freq_, Ugen_ptr amp_) :
            Ugen(id, 'b', nchans) {
        freq = freq_;
        amp = amp_;
        flags = CAN_TERMINATE;
        states.set_size(chans);
        fConst0 = 1.0f / std::min<float>(1.92e+05f, std::max<float>(1.0f, float(BR)));
        init_freq(freq);
        init_amp(amp);
    }

    ~Sineb() {
        freq->unref();
        amp->unref();
    }

    const char *classname() { return Sineb_name; }

    void initialize_channel_states() {
        for (int i = 0; i < chans; i++) {
            for (int l2 = 0; l2 < 2; l2 = l2 + 1) {
                states[i].iVec1[l2] = 0;
            }
            for (int l3 = 0; l3 < 2; l3 = l3 + 1) {
                states[i].fRec1[l3] = 0.0f;
            }
        }
    }

    void print_sources(int indent, bool print_flag) {
        freq->print_tree(indent, print_flag, "freq");
        amp->print_tree(indent, print_flag, "amp");
    }

    void repl_freq(Ugen_ptr ugen) {
        freq->unref();
        init_freq(ugen);
    }

    void repl_amp(Ugen_ptr ugen) {
        amp->unref();
        init_amp(ugen);
    }

    void set_freq(int chan, float f) {
        freq->const_set(chan, f, "Sineb::set_freq");
    }

    void set_amp(int chan, float f) {
        amp->const_set(chan, f, "Sineb::set_amp");
    }

    void init_freq(Ugen_ptr ugen) { init_param(ugen, freq, &freq_stride); }

    void init_amp(Ugen_ptr ugen) { init_param(ugen, amp, &amp_stride); }

    void real_run() {
        freq_samps = freq->run(current_block);  // update input
        amp_samps = amp->run(current_block);  // update input
        Sineb_state *state = &states[0];
        for (int i = 0; i < chans; i++) {
            float fSlow0 = float(amp_samps[0]);
            float fSlow1 = fConst0 * float(freq_samps[0]);
            state->iVec1[0] = 1;
            float fTemp0 = ((1 - state->iVec1[1]) ? 0.0f : fSlow1 + state->fRec1[1]);
            state->fRec1[0] = fTemp0 - std::floor(fTemp0);
            out_samps[0] = FAUSTFLOAT(fSlow0 * ftbl0SinebSIG0[std::max<int>(0, std::min<int>(int(65536.0f * state->fRec1[0]), 65535))]);
            state->iVec1[1] = state->iVec1[0];
            state->fRec1[1] = state->fRec1[0];
    
            state++;
            out_samps++;
            freq_samps += freq_stride;
            amp_samps += amp_stride;
        }
    }
};

// ************ blend *************
/* blend -- unit generator for arco
 *
 * adapted from multx, Oct 2024
 *
 * blend is a selector using a signal to select or mix 2 inputs
 *
 * The blend parameter can be provided with an initial value like multx.
 * 
 * To cut down on the combinatorics of different rates, if only one of
 * x1 or x2 is audio rate, we swap them. In that case, the blend 
 * parameter b must become 1 - b, which is indicated by swap_x1_x2.
 */

extern const char *Blend_name;

class Blend : public Ugen {
public:
    struct Blend_state {
        Sample prev_x1_gain;
        Sample prev_x2_gain;
        Sample prev_b;  // blend
    };
    Vec<Blend_state> states;
    void (Blend::*run_channel)(Blend_state *state);

    Ugen_ptr x1;
    int x1_stride;
    Sample_ptr x1_samps;

    Ugen_ptr x2;
    int x2_stride;
    Sample_ptr x2_samps;

    Ugen_ptr b;
    int b_stride;
    Sample_ptr b_samps;

    float gain;
    int mode;

    Blend(int id, int nchans, Ugen_ptr x1_, Ugen_ptr x2_, Ugen_ptr b_, 
          float b_init, int mode_) :
            Ugen(id, 'a', nchans) {
        x1 = x1_;
        x2 = x2_;
        b = b_;
        mode = mode_;
        gain = 1;
        flags = CAN_TERMINATE;
        states.set_size(chans);  // zeros everything

        init_x1(x1);
        init_x2(x2);
        init_b(b);
        update_run_channel();
        // normally, Arco Ugens reinitialize channel states whenever
        // a new input has a different rate, changing the run_channel
        // method. In this special case, reverting to the starting
        // value for x2 (x2_init) after some computation makes no
        // sense, so we only initialize state here at the beginning.
        initialize_channel_states(b_init);
    }

    ~Blend() {
        x1->unref();
        x2->unref();
        b->unref();
    }

    const char *classname() { return Blend_name; }

    void initialize_channel_states(float b_init) {
        for (int i = 0; i < chans; i++) {
            states[i].prev_b = b_init;
        }
    }

    void update_run_channel() {
        if (mode == BLEND_POWER) {
            run_channel = &Blend::power_aab_a;
        } else if (mode == BLEND_45) {
            run_channel = &Blend::p45_aab_a;
        } else {
            run_channel = &Blend::linear_aab_a;
        }
    }


    void print_sources(int indent, bool print_flag) {
        x1->print_tree(indent, print_flag, "x1");
        x2->print_tree(indent, print_flag, "x2");
        b->print_tree(indent, print_flag, "b");
    }

    void repl_x1(Ugen_ptr ugen) {
        x1->unref();
        init_x1(ugen);
        update_run_channel();
    }

    void repl_x2(Ugen_ptr ugen) {
        x2->unref();
        init_x2(ugen);
        update_run_channel();
    }

    void repl_b(Ugen_ptr ugen) {
        b->unref();
        init_b(ugen);
        update_run_channel();
    }

    void set_x1(int chan, float f) {
        x1->const_set(chan, f, "Blend::set_x1");
    }

    void set_x2(int chan, float f) {
        x2->const_set(chan, f, "Blend::set_x2");
    }

    void set_b(int chan, float f) {
        b->const_set(chan, f, "Blend::set_x2");
    }

    void init_x1(Ugen_ptr ugen) {
        if (ugen->rate != 'a') {
            /* ugen = new Upsample(-1, ugen->chans, ugen); */
            fail();
        }
        init_param(ugen, x1, &x1_stride); 
    }

    void init_x2(Ugen_ptr ugen) {
        if (ugen->rate != 'a') {
            /* ugen = new Upsample(-1, ugen->chans, ugen); */
            fail();
        }
        init_param(ugen, x2, &x2_stride);
    }

    void init_b(Ugen_ptr ugen) {
        if (ugen->rate == 'a') {
            /* ugen = new Dnsampleb(-1, ugen->chans, ugen, LOWPASS500); */
            fail();
        }
        init_param(ugen, b, &b_stride); 
    }

    
#define LINEAR_BLEND(out, x1, x2, b) \
            (out) = gain * ((x1) * (1.0f - (b)) + (x2) * (b))

#define COMPUTE_COS_SIN(b) \
    float angle = (COS_TABLE_SIZE + 1) - ((b) * (COS_TABLE_SIZE / 2.0)); \
    int anglei = (int) angle; \
    float x1_gain = raised_cosine[anglei]; \
    x1_gain += (angle - anglei) * (x1_gain - raised_cosine[anglei + 1]); \
    x1_gain += x1_gain - 1;  /* convert raised cos to cos */ \
    angle = (COS_TABLE_SIZE * 3 / 2.0f) - angle; \
    anglei = (int) angle; \
    float x2_gain = raised_cosine[anglei]; \
    x2_gain += (angle - anglei) * (x2_gain - raised_cosine[anglei + 1]); \
    x2_gain += x2_gain - 1;

#define COMPUTE_X1_X2_GAIN \
    Sample x1_gain_fast = state->prev_x1_gain; \
    Sample x1_gain_incr = (x1_gain - x1_gain_fast) * BL_RECIP; \
    state->prev_x1_gain = x1_gain; \
    Sample x2_gain_fast = state->prev_x2_gain; \
    Sample x2_gain_incr = (x2_gain - x2_gain_fast) * BL_RECIP; \
    state->prev_x2_gain = x2_gain;

#define P45_BLEND \
    x1_gain = sqrt((1 - b) * x1_gain);  /* blend linear with constant power */ \
    x2_gain = sqrt((1 - b) * x2_gain);  /* blend linear with constant power */ \

#define APPLY_X1_X2_GAIN \
    for (int i = 0; i < BL; i++) { \
        x1_gain_fast += x1_gain_incr; \
        x2_gain_fast += x2_gain_incr; \
        *out_samps++ = gain * (x1_samps[i] * x1_gain_fast + \
                               x2_samps[i] * x2_gain_fast); \
    }


    void linear_aab_a(Blend_state *state) {
        float b = *b_samps;
        Sample b_fast = state->prev_b;
        Sample b_incr = (b - b_fast) * BL_RECIP;
        state->prev_b = b;
        for (int i = 0; i < BL; i++) {
            b_fast += b_incr;
            *out_samps++ = gain * (x1_samps[i] * (1.0f - b_fast) +
                                   x2_samps[i] * b_fast);
        }
    }

    void power_aab_a(Blend_state *state) {
        float b = *b_samps;
        COMPUTE_COS_SIN(b);  // compute cos and sin terms from b
        COMPUTE_X1_X2_GAIN  // prepare to upsample x1_gain, x2_gain
        APPLY_X1_X2_GAIN  // compute output using x1_gain, x2_gain weighting
    }

    void p45_aab_a(Blend_state *state) {
        float b = *b_samps;
        COMPUTE_COS_SIN(b);  // compute cos and sin terms from b
        P45_BLEND  // blend const power gains with linear gains
        COMPUTE_X1_X2_GAIN  // prepare to upsample x1_gain, x2_gain
        APPLY_X1_X2_GAIN  // compute output using x1_gain, x2_gain weighting
    }


    void real_run() {
        x1_samps = x1->run(current_block); // update input
        x2_samps = x2->run(current_block); // update input
        b_samps = b->run(current_block);  // update input
        if ((x1->flags & x2->flags & TERMINATED) &&
            (flags & CAN_TERMINATE)) {
            terminate(ACTION_TERM);
        }
        Blend_state *state = &states[0];
        for (int i = 0; i < chans; i++) {
            (this->*run_channel)(state);
            state++;
            x1_samps += x1_stride;
            x2_samps += x2_stride;
            b_samps += b_stride;
        }
    }
};


const char *Blend_name = "Blend";

// *********** stpan ************
/* stpan.h -- unit generator that distributes inputs across stereo field
 *
 * Roger B. Dannenberg
 * Jan 2025
 */

const char *Stpan_name = "Stpan";

typedef struct Stpan_state {
    Sample left;
    Sample right;
} Stpan_state;


class Stpan : public Ugen {
public:
    Vec<Stpan_state> states;

    Ugen_ptr x;
    int x_stride;
    Sample_ptr x_samps;

    Ugen_ptr pan;
    int pan_stride;
    Sample_ptr pan_samps;

    Stpan(int id, int nchans, Ugen_ptr x_, Ugen_ptr pan_) :
        Ugen(id, 'a', 2 /* chans */) {
        x = x_;
        pan = pan_;
        init_x(x);
        init_pan(pan);
        states.set_size(nchans);
    }

    void repl_x(Ugen_ptr ugen) {
        x->unref();
        init_x(ugen);
    }

    void repl_pan(Ugen_ptr ugen) {
        pan->unref();
        init_pan(ugen);
    }

    void init_x(Ugen_ptr ugen) { init_param(ugen, x, &x_stride); }

    void init_pan(Ugen_ptr ugen) { init_param(ugen, pan, &pan_stride); }


    ~Stpan() {
        x->unref();
        pan->unref();
    }

    const char *classname() { return Stpan_name; }


    void print_sources(int indent, bool print_flag) {
        fail();
    }


    void real_run() {
        x_samps = x->run(current_block);
        pan_samps = pan->run(current_block);
        if ((x->flags & TERMINATED) && (flags & CAN_TERMINATE)) {
            terminate(ACTION_TERM);
        }
        // first channel to write output
        Stpan_state *state = &states[0];
        float pan_sig = *pan_samps;
        pan_sig = (pan_sig < 0 ? 0 : (pan_sig > 1 ? 1 : pan_sig));
        // pan 0 to 1 maps to COS_TABLE_SIZE + 2 to COS_TABLE_SIZE / 2 + 2
        float angle = (COS_TABLE_SIZE + 2) - 
                      (pan_sig * (COS_TABLE_SIZE / 2.0));
        int anglei = (int) angle;
        float left = raised_cosine[anglei];
        left += (angle - anglei) * (left - raised_cosine[anglei + 1]);
        // now left is from 0.5 to 1 because we used raised_cosine,
        // but we want 0 to 1 as in cosine, so fix it
        left += left - 1;
        float left_incr = (left - state->left) * BL_RECIP;
        left = state->left;
        
        // pan 0 to 1 maps to COS_TABLE_SIZE / 2 + 2 to COS_TABLE_SIZE + 2
        angle = (COS_TABLE_SIZE * 3 / 2.0f) - angle;
        anglei = (int) angle;
        float right = raised_cosine[anglei];
        right += (angle - anglei) * (right - raised_cosine[anglei + 1]);
        // now right is from 0.5 to 1 because we used raised_cosine,
        // but we want 0 to 1 as in cosine, so fix it
        right += right - 1;
        float right_incr = (right - state->right) * BL_RECIP;
        right = state->right;

        for (int i = 0; i < BL; i++) {
            left += left_incr;
            right += right_incr;
            out_samps[i] = *x_samps * left;
            out_samps[i + BL] = *x_samps++ * right;
        }
        state->left = left;
        state->right = right;

        state++;
        x_samps += x_stride;
        pan_samps += pan_stride;

        for (int i = 1; i < x->chans; i++) {  // sum remaining chans into outputs
            for (int i = 0; i < BL; i++) {
                fail();  // not implemented! (similar to above)
            }
            state++;
            x_samps += x_stride;
            pan_samps += pan_stride;
        }
    }
};

typedef Stpan *Stpan_ptr;


// ********** Sum ***********
/* sum.h -- unit generator that sums inputs
 *
 * Roger B. Dannenberg
 * Nov 2023
 */

const char *Sum_name = "Sum";

class Sum : public Ugen {
public:
    float gain;
    float prev_gain;
    bool wrap;

    Vec<Ugen_ptr> inputs;

    Sum(int id, int nchans, int wrap_) : Ugen(id, 'a', nchans) {
        gain = 1;
        prev_gain = 1;
        wrap = (wrap_ != 0); };

    ~Sum() {
        for (int i = 0; i < inputs.size(); i++) {
            inputs[i]->unref();
        }
        // since inputs is a member, ~Vec will run now and delete it
    }

    const char *classname() { return Sum_name; }


    void print_sources(int indent, bool print_flag) {
        for (int i = 0; i < inputs.size(); i++) {
            char name[8];
            snprintf(name, 8, "%d", i);
            inputs[i]->print_tree(indent, print_flag, name);
        }
    }


    // insert operation takes a signal and a gain
    void ins(Ugen_ptr input) {
        assert(input->chans > 0);
        if (input->rate != 'a') {
            arco_warn("sum_ins: input rate is not 'a', ignore insert");
            return;
        }
        int i = find(input, false);
        if (i < 0) {  // input is not already in sum; append it
            inputs.push_back(input);
            input->ref();
        }
        /*
        printf("After insert, sum inputs (%p) has\n", inputs.get_array());
        for (i = 0; i < inputs.size(); i++) {
            printf("    %p: %s\n", inputs[i], inputs[i]->classname());
        }
        */
    }


    // find the index of Ugen input -- linear search
    int find(Ugen_ptr input, bool expected = true) {
        for (int i = 0; i < inputs.size(); i++) {
            if (inputs[i] == input) {
                return i;
            }
        }
        if (expected) {
            arco_warn("Sum::find: %p not found, nothing was changed", input);
        }
        return -1;
    }
        

    // remove operation finds the signal and removes it and its gain
    void rem(Ugen_ptr input) {
        int i = find(input);
        if (i >= 0) {
            inputs[i]->unref();
            inputs.remove(i);
        }
    }


    void swap(Ugen_ptr ugen, Ugen_ptr replacement) {
        int loc = find(ugen, false);
        if (loc == -1) {
            arco_warn("/arco/sum/swap id (%d) not in output set, ignored\n",
                      id);
            return;
        }
        ugen->unref();
        inputs[loc] = replacement;
        replacement->ref();
    }


    void real_run() {
        int starting_size = inputs.size();
        int i = 0;
        bool copy_first_input = true;
        while (i < inputs.size()) {
            Ugen_ptr input = inputs[i];
            Sample_ptr input_ptr = input->run(current_block);
            if (input->flags & TERMINATED) {
                send_action_id(ACTION_REM, input->id);
                input->unref();
                inputs.remove(i);
                continue;
            }
            i++;
            int ch = input->chans;
            if (copy_first_input) {
                block_copy_n(out_samps, input_ptr, MIN(ch, chans));
                if (ch < chans) {  // if more output channels than
                    // input channels, zero fill; but if there is only one
                    // input channel, copy it to all outputs
                    //if (ch == 1) {
                    //    for (int i = 1; i < n; i++) {
                    //        block_copy(out_samps + BL * i, input_ptr);
                    //    }
                    //} else {
                    block_zero_n(out_samps + BL * ch, chans - ch);
                    //}
                }
                copy_first_input = false;  // from now on, need to sum input
            } else {
                block_add_n(out_samps, input_ptr, MIN(ch, chans));
            }
            // whether we copied or sumed the first chans channels of input,
            // there could be extra channels to "wrap" and now we have to sum
            if (ch > chans && wrap) {
                for (int c = chans; c < ch; c += chans) {
                    block_add_n(out_samps, input_ptr + c * BL,
                                MIN(ch - c, chans));
                }
            }
        }
        if (copy_first_input) {  // did not find even first input to copy
            block_zero_n(out_samps, chans);  // zero the outputs
            // Check starting_size so that if we entered real_run() with no
            // inputs, we will not terminate. Only terminate if there was at
            // least one input that terminated and now there are none:
            if (starting_size > 0 && (flags & CAN_TERMINATE)) {
                terminate(ACTION_TERM);
            }
        }
        // scale output by gain. Gain change is limited to at least 50 msec
        // for a full-scale (0 to 1) change, and special cases of constant
        // gain and unity gain are implemented:
        float gincr = (gain - prev_gain) * BL_RECIP;
        float abs_gincr = fabs(gincr);
        if (abs_gincr < 1e-6) {
            if (gain != 1) {
                for (int i = 0; i < chans * BL; i++) {
                    *out_samps++ *= gain;
                }
                prev_gain = gain;
            }
        } else {
            // we want abs_gincr * 0.050 * AR < 1, so abs_gincr < AP / 0.050
            if (abs_gincr > AP / 0.050) {
                gincr = copysignf(AP / 0.050, gincr);  // copysign(mag, sgn)
                // apply ramp to each channel:
                float g;
                for (int ch = 0; ch < chans; ch++) {
                    g = prev_gain;
                    for (int i = 0; i < BL; i++) {
                        g += gincr;
                        *out_samps++ *= g;
                    }
                }
                // due to rate limiting, the end of the ramp over BL
                // samples may not reach gain, so prev_gain is set to g
                prev_gain = g;
            }
        }
    }
};

typedef Sum *Sum_ptr;


// ********** Mathb **********
/* mathb -- unit generator for arco for binary mathb operations
 * Roger B. Dannenberg
 * Nov, 2023
 */


const char *Mathb_name = "Mathb";

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

              case MATH_OP_QNT: {
                    Sample x2 = x2_samps[i];
                    Sample q = x2 * 0x8000;
                    *out_samps++ = x2 <= 0 ? 0 : 
                            round((x1_samps[i] + 1) * q) / q - 1;
                }
                break;

              case MATH_OP_TAN: {
                    *out_samps++ = tanf(x1_samps[i] * x2_samps[i]);
                }
                break;

              case MATH_OP_ATAN2: {
                    *out_samps++ = atan2f(x1_samps[i], x2_samps[i]);
                }
                break;

              case MATH_OP_SIN: {
                    *out_samps++ = sinf(x1_samps[i] * x2_samps[i]);
                }
                break;

              case MATH_OP_COS: {
                    *out_samps++ = cosf(x1_samps[i] * x2_samps[i]);
                }
                break;

              default: 
                fail();
                break;
            }
            x1_samps += x1_stride;
            x2_samps += x2_stride;
        }
    }
};


Pwlb_ptr vibamp_pwlb;
Pwlb_ptr vibfreq_pwlb;
Ugen_ptr vibosc_sineb;
Pwlb_ptr freq_pwlb;
Ugen_ptr modfreq_mathb;
Ugen_ptr unity_const;
Tableosc_ptr osc1_tableosc;
Tableosc_ptr osc2_tableosc;
Pwlb_ptr xfade_pwlb;
Ugen_ptr blend;
Pwlb_ptr gain_pwlb;
Pwlb_ptr pan_pwlb;
Stpan_ptr pan;
Sum_ptr sum;

float spectrum1[] = {1.0, 0.5, 0.25, 0.12};
float spectrum2[] = {1.0, 0.1, 0.01, 0.01};

void build_graph()
{
    vibamp_pwlb = new Pwlb(10);
    vibamp_pwlb->points.clear();
    vibamp_pwlb->point(AR * 2);
    vibamp_pwlb->point(10.0);
    vibamp_pwlb->start();

    vibfreq_pwlb = new Pwlb(11);
    vibfreq_pwlb->points.clear();
    vibfreq_pwlb->point(AR * 1);
    vibfreq_pwlb->point(6.0);
    vibfreq_pwlb->start();

    vibosc_sineb = new Sineb(12, 1, vibfreq_pwlb, vibamp_pwlb);

    freq_pwlb = new Pwlb(13);;
    freq_pwlb->points.clear();
    freq_pwlb->set(440.0);
    freq_pwlb->point(AR * SIMSECS);
    freq_pwlb->point(440.0);
    freq_pwlb->start();

    modfreq_mathb = new Mathb(14, 1, MATH_OP_ADD, freq_pwlb, vibosc_sineb);

    gain_pwlb = new Pwlb(20);
    gain_pwlb->points.clear();
    gain_pwlb->point(AR * 1);
    gain_pwlb->point(1.0);
    gain_pwlb->start();

    // unity_const = new Const(15, 1, 1.0);

    osc1_tableosc = new Tableosc(16, 1, modfreq_mathb, gain_pwlb, 0.0);
    osc1_tableosc->create_table(0, 2048, 4, spectrum1, false);

    osc2_tableosc = new Tableosc(17, 1, modfreq_mathb, gain_pwlb, 0.0);
    osc2_tableosc->create_table(0, 2048, 4, spectrum2, false);

    xfade_pwlb = new Pwlb(18);
    xfade_pwlb->points.clear();
    xfade_pwlb->point(AR * SIMSECS);
    xfade_pwlb->point(1.0);
    xfade_pwlb->start();

    blend = new Blend(19, 1, osc1_tableosc, osc2_tableosc, xfade_pwlb, 0.0,
                  BLEND_LINEAR);

    pan_pwlb = new Pwlb(21);
    pan_pwlb->points.clear();
    pan_pwlb->set(0.4);
    pan_pwlb->point(AR * SIMSECS);
    pan_pwlb->point(0.6);
    pan_pwlb->start();

    pan = new Stpan(22, 2, blend, pan_pwlb);

    sum = new Sum(23, 2, 0);
    sum->ins(pan);
}

int main()
{
    o2_internet_enable(false);
    o2_initialize("arcobenchmark");
    o2_clock_set(NULL, NULL);
    ugen_initialize();
    build_graph();
    printf("Initialization complete\n");
    O2time start_time = o2_local_time();
    
    int block_count = 0;
    int duration_blocks = BR * SIMSECS;
    while (block_count < duration_blocks) {
        block_count += 1;
        sum->run(block_count);
        // printf("%ld\n", real_run_count);
    }

    O2time finish_time = o2_local_time();

    printf("Completed %d samples, %g seconds, wall time %g s.\n",
           duration_blocks * BL, duration_blocks * BP,
           finish_time - start_time);
    printf("real_run methods were called %ld times\n"
           "  which is %g times per block.\n", real_run_count,
           (double) real_run_count / block_count);
}
