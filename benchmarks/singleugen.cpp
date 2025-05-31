// singleugen.cpp - based on Dannenberg and Thompson, CMJ, implements
//     computation in one giant ugen
//
// Roger B. Dannenberg
// Jan 2025

// #define EXTRA 1

/* BENCHMARK UGEN CALCULATION TREE

sum: Sum
  pan: Stpan
    pan_pwlb: Pwlb
    blend: Blend
      xfade_pwlb: Pwlb
      osc1_tableosc: Tableosc
         gain_pwlb: Pwlb
         modfreq_mathb: Mathb
             freq_pwlb: Pwlb
             vibosc_sineb: Sineb
                 vibfreq_pwlb: Pwlb
                 vibamp_pwlb: Pwlb
      osc2_tableosc: Tableosc
          [same as osc1_tableosc]

 */

#define SIMSECS 3600

#include <climits>
#include "o2internal.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <math.h>

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif

#define SINESIZE 4096

class SineSIG0 {
    
  private:
    
    int iVec0[2];
    int iRec0[2];
    
  public:
    
    int getNumInputsSineSIG0() {
        return 0;
    }
    int getNumOutputsSineSIG0() {
        return 1;
    }
    
    void instanceInitSineSIG0(int sample_rate) {
        for (int l0 = 0; l0 < 2; l0 = l0 + 1) {
            iVec0[l0] = 0;
        }
        for (int l1 = 0; l1 < 2; l1 = l1 + 1) {
            iRec0[l1] = 0;
        }
    }
    
    void fillSineSIG0(int count, float* table) {
        for (int i1 = 0; i1 < count; i1 = i1 + 1) {
            iVec0[0] = 1;
            iRec0[0] = (iVec0[1] + iRec0[1]) % SINESIZE;
            table[i1] = std::sin(9.58738e-05f * float(iRec0[0]));
            iVec0[1] = iVec0[0];
            iRec0[1] = iRec0[0];
        }
    }

};


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

// *********** arco compatibility *************

#define arco_print printf
#define arco_warn printf
#define arco_error printf
void fail()
{
    printf("FATAL ERROR!\n");
    exit(1);
}

#define NUM_MATH_OPS 1
#define NUM_UNARY_OPS 1

#include <cmath>
#include "ugen.h"

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

float spectrum1[] = {1.0, 0.5, 0.25, 0.12};
float spectrum2[] = {1.0, 0.1, 0.01, 0.01};

struct Pwlb_struct {
    float current;   // value of the next output sample
    int seg_togo;    // remaining samples in current segment
    float seg_incr;  // increment from one sample to the next in current segment
    float final_value; // target value that will be first sample of next segment
    int next_point_index;  // index into point of the next segment
    int action_id;         // send this when envelope is done
    Vec<float> points;     // envelope breakpoints (seg_len, final_value)
} vibamp, vibfreq, f0, gain, xfade, panamt;


void pwlb_init(Pwlb_struct *pwlb)
{
    pwlb->current = 0.0f;
    pwlb->seg_togo = INT_MAX;
    pwlb->seg_incr = 0.0;
    pwlb->final_value = 0.0f;
    pwlb->next_point_index = 0;
    pwlb->action_id = 0;
    pwlb->points.init(0);
}

void pwlb_start(Pwlb_struct *pwlb)
{
    pwlb->next_point_index = 0;
    pwlb->seg_togo = 0;
    pwlb->final_value = pwlb->current;
}


void pwlb_segment_setup(Pwlb_struct *pwlb) {
    // using while allows durations of zero so that we can have an
    // initial non-zero value by setting initial duration to 0:
    while (pwlb->seg_togo == 0) { // set up next segment
        pwlb->current = pwlb->final_value;  // make output value exact
        if (pwlb->next_point_index >= pwlb->points.size()) {
            pwlb->seg_incr = 0.0f;
            break;
        } else {
            pwlb->seg_togo = (int) pwlb->points[pwlb->next_point_index++];
            pwlb->final_value = pwlb->points[pwlb->next_point_index++];
            pwlb->seg_incr = (pwlb->final_value - pwlb->current) /
                             pwlb->seg_togo;
        }
    }
}

#define PWLB_LOCALS(pwlb) float pwlb ## _output;

#define RUN_PWLB(pwlb)       \
    if (pwlb.seg_togo == 0) { \
        pwlb_segment_setup(&pwlb); } \
    pwlb ## _output = pwlb.current; \
    pwlb.current += pwlb.seg_incr; \
    pwlb.seg_togo--;


struct Mathb_struct {
    int count;
    Sample prev;
    Sample hold;
    int op;
    Sample output;
} modfreq_mathb_struct;


static SineSIG0* newSineSIG0() { return (SineSIG0*)new SineSIG0(); }
static void deleteSineSIG0(SineSIG0* dsp) { delete dsp; }

static float ftbl0SineSIG0[SINESIZE];

struct Sineb_struct {
    int iVec1[2];
    float fRec1[2];
    float fConst0;
} vibosc;


void sineb_init(Sineb_struct *sineb)
{
    sineb->fConst0 = 1.0f / std::min<float>(1.92e+05f,
                                            std::max<float>(1.0f, float(BR)));
    sineb->iVec1[0] = 0;
    sineb->iVec1[1] = 0;
    sineb->fRec1[0] = 0.0f;
    sineb->fRec1[1] = 0.0f;
}    


#define SINEB_LOCALS(sineb) float sineb ## _output;

#define RUN_SINEB(sineb, freq, amp) {               \
    float fSlow0 = amp; \
    float fSlow1 = sineb.fConst0 * freq; \
    sineb.iVec1[0] = 1;  \
    float fTemp0 = ((1 - sineb.iVec1[1]) ? 0.0f : fSlow1 + sineb.fRec1[1]);  \
    sineb.fRec1[0] = fTemp0 - std::floor(fTemp0);  \
    sineb ## _output = FAUSTFLOAT(fSlow0 * ftbl0SineSIG0[  \
            std::max<int>(0, std::min<int>(int(SINESIZE * sineb.fRec1[0]),  \
                                           (SINESIZE - 1)))]);          \
    sineb.iVec1[1] = sineb.iVec1[0];  \
    sineb.fRec1[1] = sineb.fRec1[0]; }


struct Tableosc_struct {
    Vec<float> wavetable;
    double phase;  // from 0 to 1 (1 represents 2PI or 360 degrees)
    Sample prev_amp;
    int which_table;
    Sample output;
} osc1, osc2
#ifdef EXTRA
    , osc3
#endif    
    ;


void tableosc_init(Tableosc_struct *tableosc)
{
    tableosc->which_table = 0;
    tableosc->phase = 0;
    tableosc->prev_amp = 0;
}


#define SCHROEDER_PHASE(n, slen) (M_PI * ((n) + 1) * (n) / (slen))


void tableosc_create_table(Tableosc_struct *tableosc, int tlen,
                           int slen, float *spec)
{
    tableosc->wavetable.init(0);
    tableosc->wavetable.set_size(tlen + 2, false);
    tableosc->wavetable.zero();
    printf("create_table size %d\n", tableosc->wavetable.size());
    int harm = 1;
    for (int h = 0; h < slen; h += 1) {
        float *data = tableosc->wavetable.get_array();
        float amp = spec[h];
        double phase = SCHROEDER_PHASE(h, slen);
        double phase_incr = (double) harm * sine_table_len / tlen;
        for (int i = 0; i < tlen + 2; i++) {
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


#define TABLEOSC_LOCALS(tableosc) \
    Sample *tableosc ## _table;  \
    int tableosc ## _tlen; \
    double tableosc ## _phase_incr; \
    float tableosc ## _amp_fast; \
    float tableosc ## _amp_incr; \
    float tableosc ## _output;


#define RUN_TABLEOSC_B(tableosc, freq_samp, amp_samp) \
    tableosc ## _tlen = tableosc.wavetable.size() - 2; \
    if (tableosc ## _tlen < 2) { \
        printf("ERROR: _tlen %d; early return\n", tableosc ## _tlen); \
        return; } \
    tableosc ## _table = tableosc.wavetable.get_array(); \
    tableosc ## _phase_incr = freq_samp * AP; \
    tableosc ## _amp_fast = tableosc.prev_amp; \
    tableosc.prev_amp = amp_samp; \
    tableosc ## _amp_incr = (amp_samp - tableosc ## _amp_fast) * BL_RECIP;


#define RUN_TABLEOSC_A(tableosc)  \
    { \
        float x = tableosc.phase * tableosc ## _tlen; \
        int ix = x; \
        float frac = x - ix; \
        tableosc ## _amp_fast += tableosc ## _amp_incr; \
        tableosc ## _output = (tableosc ## _table[ix] * (1 - frac) +  \
                tableosc ## _table[ix + 1] * frac) * tableosc ## _amp_fast; \
        tableosc.phase += tableosc ## _phase_incr; \
        while (tableosc.phase > 1) tableosc.phase--; \
        while (tableosc.phase < 0) tableosc.phase++; \
    }


struct Blend_struct {
    Sample prev_x1_gain;
    Sample prev_x2_gain;
    Sample b;
    Sample prev_b;  // blend
    float gain;
    Sample output;
} blender;


void blend_init(Blend_struct *blend, float b_init)
{
    blend->gain = 1;
    blend->prev_x1_gain = 0.0f;
    blend->prev_x2_gain = 0.0f;
    blend->prev_b = b_init;
}


#define BLEND_LOCALS(blend) \
    float blend ## _b_fast; \
    float blend ## _b_incr; \
    float blend ## _output;


#define RUN_BLEND_B(blend, b)          \
    blend ## _b_fast = blend.prev_b; \
    blend ## _b_incr = (b - blend ## _b_fast) * BL_RECIP; \
    blend.prev_b = b;


#define RUN_BLEND_A(blend, x1_samp, x2_samp, blend_samp) \
    blend ## _b_fast += blend ## _b_incr; \
    blend ## _output = blend.gain * (x1_samp * (1.0f - blend ## _b_fast) + \
                                    x2_samp * blend ## _b_fast);


struct Stpan_struct {
    Sample left;
    Sample right;
} panner;


void stpan_init(Stpan_struct *pan)
{
}

#define STPAN_LOCALS(stpan) \
    float stpan ## _output; \
    float stpan ## _left_incr; \
    float stpan ## _right_incr; \
    float stpan ## _output_left; \
    float stpan ## _output_right;

    

#define RUN_STPAN_B(stpan, pan_samp) \
{ \
    float pan_sig = pan_samp; \
    pan_sig = (pan_sig < 0 ? 0 : (pan_sig > 1 ? 1 : pan_sig)); \
    float angle = (COS_TABLE_SIZE + 2) - \
                  (pan_sig * (COS_TABLE_SIZE / 2.0)); \
    int anglei = (int) angle; \
    float left = raised_cosine[anglei]; \
    left += (angle - anglei) * (left - raised_cosine[anglei + 1]); \
    left += left - 1; \
    stpan ## _left_incr = (left - stpan.left) * BL_RECIP; \
    angle = (COS_TABLE_SIZE * 3 / 2.0f) - angle; \
    anglei = (int) angle; \
    float right = raised_cosine[anglei]; \
    right += (angle - anglei) * (right - raised_cosine[anglei + 1]); \
    right += right - 1; \
    stpan ## _right_incr = (right - stpan.right) * BL_RECIP; }


#define RUN_STPAN_A(stpan, x_samp) \
    stpan.left += stpan ## _left_incr; \
    stpan.right += stpan ## _right_incr; \
    stpan ## _output_left = x_samp * stpan.left; \
    stpan ## _output_right = x_samp * stpan.right;


struct Stsum_struct {
    float gain;
    float prev_gain;
    Sample output[BL * 2];
} stsum;


void stsum_init(Stsum_struct *sum)
{
    block_zero_n(sum->output, 2);
    sum->gain = 1;
    sum->prev_gain = 1;
}


void stsum_finish(Stsum_struct *sum)
{
    // scale output by gain. Gain change is limited to at least 50 msec
    // for a full-scale (0 to 1) change, and special cases of constant
    // gain and unity gain are implemented:
    float gincr = (sum->gain - sum->prev_gain) * BL_RECIP;
    float abs_gincr = fabs(gincr);
    float *out_samps = sum->output;
    if (abs_gincr < 1e-6) {
        if (sum->gain != 1) {
            for (int i = 0; i < 2 * BL; i++) {
                *out_samps++ *= sum->gain;
            }
            sum->prev_gain = sum->gain;
        }
    } else {
        // we want abs_gincr * 0.050 * AR < 1, so abs_gincr < AP / 0.050
        if (abs_gincr > AP / 0.050) {
            gincr = copysignf(AP / 0.050, gincr);  // copysign(mag, sgn)
            // apply ramp to each channel:
            float g;
            for (int ch = 0; ch < 2; ch++) {
                g = sum->prev_gain;
                for (int i = 0; i < BL; i++) {
                    g += gincr;
                    *out_samps++ *= g;
                }
            }
            // due to rate limiting, the end of the ramp over BL
            // samples may not reach gain, so prev_gain is set to g
            sum->prev_gain = g;
        }
    }
}


#define RUN_STSUM_A(stsum, left, right) \
    stsum.output[i] += left; \
    stsum.output[BL + i] += right;


void initialize_all()
{
    pwlb_init(&vibamp);
    vibamp.points.clear();
    vibamp.points.push_back(BR * 2);
    vibamp.points.push_back(10.0);
    pwlb_start(&vibamp);

    pwlb_init(&vibfreq);
    vibfreq.points.clear();
    vibfreq.points.push_back(BR * 1);
    vibfreq.points.push_back(6.0);
    pwlb_start(&vibfreq);

    sineb_init(&vibosc);

    pwlb_init(&f0);
    f0.points.clear();
    f0.current = 440.0;
    f0.points.push_back(BR * SIMSECS);
    f0.points.push_back(440.0);
    pwlb_start(&f0);

    pwlb_init(&gain);
    gain.points.clear();
    gain.points.push_back(BR * 1);
    gain.points.push_back(1.0);
    pwlb_start(&gain);

    pwlb_init(&xfade);
    xfade.points.clear();
    xfade.points.push_back(BR * 1);
    xfade.points.push_back(1.0);
    pwlb_start(&xfade);

    pwlb_init(&panamt);
    panamt.points.clear();
    panamt.points.push_back(BR * SIMSECS);
    panamt.points.push_back(1.0);
    pwlb_start(&panamt);

    tableosc_init(&osc1);
    tableosc_create_table(&osc1, 2048, 4, spectrum1);

    tableosc_init(&osc2);
    tableosc_create_table(&osc2, 2048, 4, spectrum2);

#ifdef EXTRA
    tableosc_init(&osc3);
    tableosc_create_table(&osc3, 2048, 4, spectrum2);
#endif

    blend_init(&blender, 0.0);

    stpan_init(&panner);
}


void run_algorithm(long block_count)
{
    PWLB_LOCALS(vibamp);
    PWLB_LOCALS(vibfreq);
    SINEB_LOCALS(vibosc);
    PWLB_LOCALS(f0);
    PWLB_LOCALS(gain);
    PWLB_LOCALS(xfade);
    PWLB_LOCALS(panamt);
    TABLEOSC_LOCALS(osc1);
    TABLEOSC_LOCALS(osc2);
#ifdef EXTRA
    TABLEOSC_LOCALS(osc3);
#endif

    BLEND_LOCALS(blender);
    STPAN_LOCALS(panner);

    // start with block-rate control computation
    RUN_PWLB(vibamp);
    RUN_PWLB(vibfreq);
    RUN_SINEB(vibosc, vibfreq_output, vibamp_output);
    RUN_PWLB(f0);
    RUN_PWLB(gain);
    float pitch = f0_output + vibosc_output;
    RUN_PWLB(xfade);
    RUN_BLEND_B(blender, xfade_output);
    RUN_PWLB(panamt);
    RUN_TABLEOSC_B(osc1, pitch, gain_output);
    RUN_TABLEOSC_B(osc2, pitch, gain_output);
#ifdef EXTRA
    RUN_TABLEOSC_B(osc3, pitch, gain_output);
#endif
    RUN_STPAN_B(panner, panamt_output);

    // audio processing in one loop
    for (int i = 0; i < BL; i++) {
        RUN_TABLEOSC_A(osc1);
        RUN_TABLEOSC_A(osc2);
#ifdef EXTRA
        RUN_TABLEOSC_A(osc3);
        RUN_TABLEOSC_A(osc3);
        RUN_TABLEOSC_A(osc3);
        RUN_TABLEOSC_A(osc3);
        RUN_TABLEOSC_A(osc3);
        RUN_TABLEOSC_A(osc3);
        RUN_TABLEOSC_A(osc3);
        RUN_TABLEOSC_A(osc3);
        RUN_TABLEOSC_A(osc3);
#endif
        RUN_BLEND_A(blender, osc1_output, osc2_output, blend_output);
        RUN_STPAN_A(panner, blender_output);
        RUN_STSUM_A(stsum, panner_output_left, panner_output_right);
#ifdef EXTRA
        stsum.output[i] += osc3_output;
#endif
    }
}


int main()
{
    o2_internet_enable(false);
    o2_initialize("arcobenchmark");
    o2_clock_set(NULL, NULL);

    // class initialization code from faust:
    SineSIG0* sig0 = newSineSIG0();
    sig0->instanceInitSineSIG0(AR);
    sig0->fillSineSIG0(SINESIZE, ftbl0SineSIG0);
    deleteSineSIG0(sig0);

    // ugen_initialize();
    // build_graph();
    initialize_all();
    printf("Initialization complete\n");
    O2time start_time = o2_local_time();
    
    int block_count = 0;
    int duration_blocks = BR * SIMSECS;

    // since we don't use results, maybe the compiler is cutting corners
    // let's output value that depends on all computation
    float bigsum = 0.0f;

    while (block_count < duration_blocks) {
        block_count += 1;
        stsum_init(&stsum);
        run_algorithm(block_count);
        stsum_finish(&stsum);
        bigsum += stsum.output[0] + stsum.output[BL];
    }

    O2time finish_time = o2_local_time();

    printf("bigsum %g\n", bigsum);

    printf("Completed %d samples, %g seconds, wall time %g s.\n",
           duration_blocks * BL, duration_blocks * BP,
           finish_time - start_time);

#define RESULTS "data/singleugen.txt"
#include "report.cpp"
}
