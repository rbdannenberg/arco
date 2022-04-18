/* cmtio.h -- asynchronous terminal IO 
 * 
 * Roger B. Dannenberg
 * Jan 2022, based on ancient code from cmu midi toolkit and then nyquist
 */

#ifdef __cplusplus
extern "C" {
#endif

#define NOCHAR -2

extern int IOinputfd;
extern int IOnochar;

int IOsetup(int inputfd);
int IOcleanup(void);
int IOgetchar(void);
int IOwaitchar(void);

#ifdef __cplusplus
}
#endif
