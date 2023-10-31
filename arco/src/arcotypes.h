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
