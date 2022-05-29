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
const int BL = 32;
const float BL_RECIP = 1.0F / BL;
const double BR = AR / BL;
const double BP = 1.0 / BR;
const int BLOCK_BYTES = BL * sizeof(Sample);
const int64_t MAX_BLOCK_COUNT = ~(((int64_t) 1) << 63);
const double PI = 3.141592653589793;
const double PI2 = PI * 2;
