/* ffts_compat.h -- compatibility with ffts API
 *
 * Roger B. Dannenberg
 * May 2024
 */

#ifdef __cplusplus
extern "C" {
#endif

int fftInit(long M);
void fftFree();
void rffts(float *data, long M, long Rows);
void riffts(float *data, long M, long Rows);
int ilog2(int n);

#ifdef __cplusplus
}
#endif
