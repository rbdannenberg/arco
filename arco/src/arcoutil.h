/* arcoutil.h -- audio dsp process for Arco
 *
 * Roger B. Dannenberg
 * Dec 2021
 */


void arco_print(const char *format, ...);

void arco_warn(const char *format, ...);

void arco_error(const char *format, ...);

double hz_to_step(double hz);

double step_to_hz(double steps);

double steps_to_hzdiff(double steps, double delta_steps);

double linear_to_vel(double x);

double vel_to_linear(double vel);

double linear_to_db(double x);

double db_to_linear(double db);
