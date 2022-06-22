/* prefs - preference interface
 *
 * Roger B. Dannenberg
 * Feb 2022
 */

/* This module is an exchange between code that manages preferences
   and any Arco code that needs to know preferences.
   For example, a preference file reader might read prefs.arco and
   call the prefs_set_* functions in this module to make them known.
   Then, any Arco function can get a preference value by calling
   one of the prefs_* functions such as prefs_in_chans().
 
   Reading/writing/managing preferences is separated from access
   to preferences so that different preference systems can be
   implemented. E.g. Serpent programs should use Serpent preferences
   (prefs.srp), while an Arco server with curses interface should
   implement reading/writing to a file and editing in curses.
 */

#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "ctype.h"
#include "prefs.h"

static char p_in_name[80] = "";
static char p_out_name[80] = "";
static char p_in_id = -1;
static char p_out_id = -1;
static int p_in_chans = -1;
static int p_out_chans = -1;
static int p_buffer_size = -1;
static int p_latency_ms = -1;


char *prefs_in_name()
{
    return p_in_name;
}


char *prefs_out_name()
{
    return p_out_name;
}


// call this with each audio device name and number.
// *default is replaced with the number if the device
// name contains the preference substring.
int prefs_in_lookup(const char *name, int id)
{
    if (*p_in_name && strstr(name, p_in_name)) {
        p_in_id = id;
    }
    return p_in_id;
}


int prefs_out_lookup(const char *name, int id)
{
    if (*p_out_name && strstr(name, p_out_name)) {
        p_out_id = id;
    }
    return p_out_id;
}


int prefs_in_id() { return p_in_id; }


int prefs_out_id() { return p_out_id; }


int prefs_in_chans() { return p_in_chans; }


int prefs_out_chans() { return p_out_chans; }


int prefs_buffer_size() { return p_buffer_size; }


int prefs_latency_ms() { return p_latency_ms; }


void prefs_set_in_name(const char *name) 
{ 
    strncpy(p_in_name, name, 80);
    p_in_name[79] = 0;
}
    

void prefs_set_out_name(const char *name)
{ 
    strncpy(p_out_name, name, 80);
    p_out_name[79] = 0;
}
    

void prefs_set_latency(int latency_ms) { p_latency_ms = latency_ms; }
void prefs_set_in_chans(int chans) { p_in_chans = chans; }
void prefs_set_out_chans(int chans) { p_out_chans = chans; }
void prefs_set_buffer_size(int size) { p_buffer_size = size; }

