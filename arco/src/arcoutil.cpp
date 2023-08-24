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

void arco_print(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    char str[256];
    vsnprintf(str, 256, format, ap);
    printf("%s", str);
}


void arco_warn(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    char str[256];
    vsnprintf(str, 256, format, ap);
    fprintf(stderr, "WARNING: %s\n", str);
    fflush(stderr);
}


void arco_error(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    char str[256];
    vsnprintf(str, 256, format, ap);
    fprintf(stderr, "ERROR: %s\n", str);
    fflush(stderr);
}


#define p1 0.0577622650466621
#define p2 2.1011784386926213


double hz_to_step(double hz)
{
    return (log(hz) - p2) / p1;
}


double step_to_hz(double steps)
{
    return exp(steps * p1 + p2);
}
