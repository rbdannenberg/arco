/* nofileio.h -- file io system stub that does nothing
 *
 * Roger B. Dannenberg
 * April 2023
 */

extern bool fileio_finished;  // used by audio thread to know when fileio is shutdown
int fileio_initialize();
