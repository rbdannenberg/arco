/* arcoutil.cpp -- Arco functions, especially for audio callback
 *
 * Roger B. Dannenberg
 * Dec 2021
 */

#include <cmath>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include "arcoutil.h"
// need dbprintf and dhprintf from O2:
#include "o2.h"
#include "debug.h"

void arco_print(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    char str[256];
    vsnprintf(str, 256, format, ap);
    va_end(ap);
    printf("%s", str);
}


void arco_warn(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    char str[256];
    vsnprintf(str, 256, format, ap);
    va_end(ap);
    printf("WARNING: %s\n", str);
    fflush(stdout);
    // not sure what happens to stderr in console-based server
    // fprintf(stderr, "WARNING: %s\n", str);
    // fflush(stderr);
}


void arco_error(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    char str[256];
    vsnprintf(str, 256, format, ap);
    va_end(ap);
    printf("ERROR: %s\n", str);
    fflush(stdout);
    // not sure what happens to stderr in console-based server
    // fprintf(stderr, "ERROR: %s\n", str);
    // fflush(stderr);
}


void adprintf(const char *format, ...)
{
    va_list ap;
    char output[256];
    va_start(ap, format);
    vsnprintf(output, sizeof(output), format, ap);
    va_end(ap);
    dbprintf("%s", output);
}


void ahprintf(const char *format, ...)
{
    va_list ap;
    char output[256];
    va_start(ap, format);
    vsnprintf(output, sizeof(output), format, ap);
    va_end(ap);
    const char *save_prefix = o2_debug_prefix;
    o2_debug_prefix = "AR";
    hdprintf("%s", output);
    o2_debug_prefix = save_prefix;
}


#define p1 0.0577622650466621
#define p2 2.1011784386926213

#define log_of_10_over_20 0.1151292546497


double hz_to_step(double hz)
{
    return (log(hz) - p2) / p1;
}


double step_to_hz(double steps)
{
    return exp(steps * p1 + p2);
}

double steps_to_hzdiff(double steps, double delta_steps)
{
    return step_to_hz(steps + delta_steps) - step_to_hz(steps);
}

double linear_to_vel(double x)
{
    x = (sqrt(abs(x)) - 0.0239372) / 0.00768553;
    return x;
}

double vel_to_linear(double vel)
{
    vel = vel * 0.00768553 + 0.0239372;
    return vel * vel;
}

double linear_to_db(double x)
{
    return log(x) / log_of_10_over_20;
}

double db_to_linear(double db)
{
    return exp(log_of_10_over_20 * db);
}
