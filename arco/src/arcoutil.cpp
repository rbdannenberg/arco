/* arcoutil.cpp -- Arco functions, especially for audio callback
 *
 * Roger B. Dannenberg
 * Dec 2021
 */

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

