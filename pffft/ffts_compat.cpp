/* ffts_compat.h -- compatibility with ffts API
 *
 * Roger B. Dannenberg
 * May 2024
 */

#include "o2.h"  // for O2_MALLOCNT etc.
#include "ffts_compat.h"
#include "pffft.h"
#include "assert.h"

static PFFFT_Setup *pffft_setups[32] = {0, 0, 0, 0, 0, 0, 0, 0,
                                        0, 0, 0, 0, 0, 0, 0, 0,
                                        0, 0, 0, 0, 0, 0, 0, 0,
                                        0, 0, 0, 0, 0, 0, 0, 0};


/* prepare to FFT with log(fft size) == M
   returns 1 for success, 2 for fail
 */
int fftInit(long M)
{
    if ((M >= 0) && (M < 32)) {
        if (!pffft_setups[M]) {
            pffft_setups[M] = pffft_new_setup(1 << M, PFFFT_REAL);
        }
        return 1;
    }
}


/* clean up allocated memory - this is NOT a real-time operation
 */
void fftFree(void)
{
    for (int i = 0; i < 32; i++) {
        if (pffft_setups[i]) {
            pffft_destroy_setup(pffft_setups[i]);
            pffft_setups[i] = NULL;
        }
    }
}


/* real fft in place
 */
void rffts(float *data, long M, long Rows)
{
    float *work = (M < 14 ? NULL : O2_MALLOCNT(1 << M, float));
    pffft_transform_ordered(pffft_setups[M], data, data, work, PFFFT_FORWARD);
    if (work) {
        O2_FREE(work);
    }
}
           

/* real fft in place
 */
void riffts(float *data, long M, long Rows)
{
    int N = 1 << M;
    float Nrecip = 1.0 / N;
    float *work = (M < 14 ? NULL : O2_MALLOCNT(N, float));
    pffft_transform_ordered(pffft_setups[M], data, data, work, PFFFT_BACKWARD);
    // result is not scaled by 1/N yet:
    for (int i = 0; i < N; i++) {
        data[i] *= Nrecip;
    }
    if (work) {
        O2_FREE(work);
    }
}
           

/* compute the log2 of an integer power of 2
 */
int ilog2(int n)
{
    int m = 0;
    while (n > (0x100 << m)) m += 8;
    while (n > (1 << m)) m++;
    assert(n == (1 << m));
    return m;
}
