/* arcoutil.h -- audio dsp process for Arco
 *
 * Roger B. Dannenberg
 * Dec 2021
 */


// print to console, messages for application users,
// this should be tied into graphical user interfaces,
// and shared memory threads should send message out
// to the main program for printing to avoid spending
// time formatting, doing file io, and possibly causing
// priority inversion (but don't assume anything more
// than printf() is implemented).  No newline is printed
// automatically.
void arco_print(const char *format, ...);

// like arco_print, but precedes output with the
// text "WARNING: " and ends with a newline and fflush().
void arco_warn(const char *format, ...);

// like arco_print, but precedes output with the
// text "ERROR: " and ends with a newline and fflush().
void arco_error(const char *format, ...);

// debugging print: sends message to dbprint in O2,
// which writes message to stdout by default, but
// messages can be diverted to o2debug.log or some
// other file using the 'L' O2 debug flag.
void adprintf(const char *format, ...);

// similar to adprintf(), but uses O2's dhprintf(),
// which adds a prefix ("AR") and timestamp to message.
// Messages with multiple parts can start with ahprintf()
// and call adprintf() to continue adding text. Any
// message started with ahprintf() should end with a
// newline.
void ahprintf(const char *format, ...);

double hz_to_step(double hz);

double step_to_hz(double steps);

double steps_to_hzdiff(double steps, double delta_steps);

double linear_to_vel(double x);

double vel_to_linear(double vel);

double linear_to_db(double x);

double db_to_linear(double db);
