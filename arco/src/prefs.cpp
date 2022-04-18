/* prefs - preference read/write
 *
 * Roger B. Dannenberg
 * Feb 2022
 */

#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "ctype.h"
#include "prefs.h"

static char p_in_device[80] = "";
static char p_out_device[80] = "";
static int p_in_chans = -1;
static int p_out_chans = -1;
static int p_buffer_size = -1;
static int p_latency = -1;


// call this with each audio device name and number.
// *default is replaced with the number if the device
// name contains the preference substring.
int prefs_in_device(const char *name, int id, int *dflt)
{
    if (*p_in_device && strstr(name, p_in_device)) {
        *dflt = id;
    }
    return *dflt;
}


int prefs_out_device(const char *name, int id, int *dflt)
{
    if (*p_out_device && strstr(name, p_out_device)) {
        *dflt = id;
    }
    return *dflt;
}


int prefs_in_chans(int dflt)
{
    return (p_in_chans != -1 ? p_in_chans : dflt);
}


int prefs_out_chans(int dflt)
{ 
    return (p_latency != -1 ? p_latency : dflt);
}


int prefs_buffer_size(int dflt)
{ 
    return (p_latency != -1 ? p_latency : dflt);
}


int prefs_latency(int dflt)
{ 
    return (p_latency != -1 ? p_latency : dflt);
}


int prefs_set_in_device(const char *name) 
{ 
    strncpy(p_in_device, name, 80);
    p_in_device[79] = 0;
}
    

int prefs_set_out_device(const char *name)
{ 
    strncpy(p_out_device, name, 80);
    p_out_device[79] = 0;
}
    

int prefs_set_latency(int latency) { p_latency = latency; }
int prefs_set_in_chans(int chans) { p_in_chans = chans; }
int prefs_set_out_chans(int chans) { p_out_chans = chans; }
int prefs_set_buffer_size(int size) { p_buffer_size = size; }

