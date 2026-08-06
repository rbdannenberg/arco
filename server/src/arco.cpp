/* arco.cpp -- main program for arco server
 *
 * Roger B. Dannenberg
 * Dec 2021
 */

/* 

Arco Configuration (C)ONFIRM AND RESTART NEEDED FOR ALL OPTIONS TO TAKE EFFECT
────────────────────────────────────────────────────────────────────────
Configuration:     __________________________    Restore: _  Delete: _
New Configuration: __________________________    Copy This: _  New: _

Ensemble name:     ________________________________   Polling rate: ____
Debug flags:       ________________________________   Ref Clock:     [X]
Networking:        ___________________                O2lite enable: [X]
HTTP Port: _____ Root: _________________________________________________
MQTT Host: __________________________________________ MQTT Port:   _____
New forward O2 to OSC: _        New forward OSC to O2: _
New MIDI In to O2: _     New MIDI Out from O2: _      MIDI Refresh: _

Service: ____________________ to OSC IP: ___.___.___.___ : _____  UDP_  Del: _
Forward OSC Port: _____ Via: UDP_      to Service: ____________________ Del: _
MIDI In: ____________________________  to Service: ____________________ Del: _
MIDI Out Service: ____________________ to: ____________________________ Del: _
...

Input device:    __             Output device:   ____________
Input channels:  4              Output channels: 2
Buffer size:     71             Latency:         10
────────────────────────────────────────────────────────────────────────
   0 - Core Audio : DELL C2722DE (0 ins, 2 outs)
   1 - Core Audio : MacBook Air Microphone (1 ins, 0 outs)
   2 - Core Audio : MacBook Air Speakers (0 ins, 2 outs)
   3 - Core Audio : Microsoft Teams Audio (1 ins, 1 outs)
   4 - Core Audio : WeMeet Audio Device (2 ins, 2 outs)
   5 - Core Audio : ZoomAudioDevice (2 ins, 2 outs)
   6 - Core Audio : Aggregate Device (0 ins, 0 outs)

   Leave blank for default (default can change when devices are opened).
   Type C(onfirm) to exit with changes; or ESC to exit without changes.


Configuration file: name is arco.config
Format is as follows. All fields are quoted strings to handle spaces
and empty fields:

o2host v1.0
Configuration: <configuration> [gives current selection]
---- <configuration>
Ensemble_name: <name>
Polling_rate: <string>
Debug_flags: <flags>
Reference_clock: Y/N
Network_option: <string>
HTTP_port: <number>
HTTP_root: <string>
MQTT_host: <string>
MQTT_port: <string>
Audio_in_name: "<name>"
Audio_out_name: "<name>"
In_chans: <number>
Out_chans: <number>
Buffer_size: <number>
Latency: <number>
O2_to_OSC: <servicename> <IP> <port> UDP
OSC_to_O2: UDP <port> <servicename>
MIDI_in: <devicename> <servicename>
MIDI_out: <servicename> <devicename>
----
---- <next configuration name>
...
----
*/

#include "o2internal.h"  // need internal to offer bridge
#include <fcntl.h>
#ifndef WIN32
#include <unistd.h>
#endif
#include <string>
#include <vector>

#if defined(ARCO_CPU_MONITOR) && defined(__APPLE__)
#include <mach/mach.h>
#include <mach/thread_info.h>
#include <pthread.h>
#endif

// Undefine Windows MOUSE_MOVED before including curses to avoid conflict
#ifdef MOUSE_MOVED
#undef MOUSE_MOVED
#endif

#ifdef __linux__
#include "ncurses.h"
#else
#include "curses.h"
#endif
#include "string.h"
#include "o2atomic.h"
#include "sharedmem.h"   // o2_shmem_inst_new()
#include "arcotypes.h"
#include "arcoinit.h"
#include "ugenids.h"
#include "audioio.h"
#include "config.h"
#include "prefs.h"
// #include "svprefs.h"
#include "arcoutil.h"

using std::string;
using std::vector;

#include "fieldentry.h"
#include "termui.h"
#if USE_MIDI
  #include "portmidi.h"
  #include "midiservice.h"
#endif
#include "o2oscservice.h"
#include "arco_internal.h"

void dialog_configure();  // forward reference

#if defined(ARCO_CPU_MONITOR)
static void log_per_thread_cpu_times();
#endif

// Global singleton with the curses interface manager object:
Terminal_ui *tu = nullptr;

// Number of lines containing configuration dialog lines for mapping
// midi to o2, o2 to midi, osc to o2, o2 to osc. When zero, there is one
// blank line separating O2 fields from Audio configuration fields. When one,
// we put a block of configuration lines between O2 and Audio configuration
// fields separated by blank lines, so there is one additional blank line:
int midi_osc_line_count = 0;

/*
#include "ugen.h"
#include "zero.h"
#include "zerob.h"
#include "thru.h"
*/

vector<string> arco_device_info;

vector<string> net_options {"localhost only", "local network", "internet",
                            "wide-area discovery"};

/* host_ variables are the actual parameter *after* PortAudio opens
   an audio device. The values may not be exactly what was requested.
*/
static int host_in_chans = 0;
static int host_out_chans = 0;
static int host_in_id = -1;
static int host_out_id = -1;
static int host_latency_ms = -1;
static int host_buffer_size = -1;

/* these host_ variables are set from preferences when the server
   initializes. We use them to remember the configuration which can
   only change by restarting
 */
char host_network_option[24];
bool host_o2lite_enable = true;
char host_ensemble_name[34] = "";  // ensemble name for O2
static bool host_http_enable = true;
static int host_http_port = 8080;
static char host_http_root[120] = "";
// static char host_debug_flags[64]; -- not used

// leading 'c' means curses only, 'p' means plain terminal only

const char *help_strings[] = {
    " A - set Audio preferences",
    " B - block count messages ON/OFF",
#if defined(ARCO_CPU_MONITOR)
    " C - Toggle CPU monitor",
#endif
    " H - describe commands like this",
    " P - Save latest selections to preferences",
    " Q - Quit the program",
    " R - Reset server: deletes all unit generators",
    " S - Start or Stop",
    " t - ask audio thread to print a test message",
    " T - test tone",
    " U - Print audio ugen tree",
    NULL  // must terminate this list with NULL
};

vector<string> top_lines;
vector<string> bottom_lines;
vector<string> dialog_bottom_lines;

static bool heartbeat_enabled = false;

#if defined(ARCO_CPU_MONITOR)
static bool cpu_monitor_enabled = false;
static double cpu_monitor_last_log = 0.0;
static double cpu_monitor_interval = 5.0; // seconds
#endif

bool arco_ready = false;
bool has_curses = false;  // curses interface exists

static void host_initialize();
/*
static bool open_requested = false;
static bool close_requested = false;
static bool reset_requested = false;
static bool quit_requested = false;
*/
static int server_aud_state = IDLE;
static int server_goal_state = IDLE;
static double server_wait_since = -100.0;

static bool server_aud_is_reset = false;
#define aud_state THIS_IS_PRIVATE_TO_THE_AUDIO_SERVICE


// convert int to temporary c-string (char *), where
// -1 -> empty string (default values)
char *itos(int i)
{
    static char str[32];
    if (i == -1) {  // default
        str[0] = 0;
    } else {
        snprintf(str, sizeof(str), "%d", i);
    }
    return str;
}


char *heapify(const char *str)
{
    size_t len = strlen(str) + 1;
    char *newstr = new char[len];
    memcpy(newstr, str, len);
    return newstr;
}


/* set goal if possible and return 0. return 1 if refused. */
int set_server_goal_state(int state, const char *err_msg)
{
    if (server_goal_state == state) {
        return 0;
    }
    if (server_goal_state != server_aud_state) {
        printf("%s: A state transition is in progress, try again.\n", err_msg);
        return 1;
    }
    server_goal_state = state;
    server_wait_since = o2_local_time();
    return 0;
}


/* O2 INTERFACE: /host/act int key, int status, int uid):
   Receive action from Arco and forward to client at /actl
*/
static void host_act(O2_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int key = argv[0]->i;
    int status = argv[1]->i;
    int uid = argv[2]->i;
    // end unpack message
    o2_send_cmd("/actl/act", 0, "iii", key, status, uid);
}


/* O2 INTERFACE: /host/devinf string info;
   Receive and print an /arco/devinf reply, which looks like:
        "<id> - <api_name> : <name> (<in> ins, <out> outs)"
*/
static void host_devinfo(O2_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    char *info = argv[0]->s;
    // end unpack message
    arco_device_info.push_back(info);
}


void host_close_audio()
{
    set_server_goal_state(IDLE, "Cannot Close now");
}


/* start audio processing */
int host_open_audio()
{
    return set_server_goal_state(RUNNING, "Cannot Open now");
}


#ifdef UNUSED_CODE
void automation()
{
    static O2time start_time = -1;
    static bool event1 = false;
    static bool event2 = false;
    assert(UGEN_BASE_ID == 4);
    const int TESTTONE_ID = 4;
    const int DUP_ID = 5;  // a Thru to widen 1 to 2 channels
    const int FREQ_ID = 6;
    const int AMP_ID = 7;
    const int SINE_ID = 8;
    const int PWL_ID = 9;
    static bool auto_started = false;
    if (!auto_started && o2_status("arco") == O2_BRIDGE) {
        printf("o2_status of arco is %d\n", o2_status("arco"));
        // make graph input and output
        o2_send_cmd("/arco/zero/new", 0, "i", ZERO_ID);
        o2_send_cmd("/arco/zerob/new", 0, "i", ZEROB_ID);
        o2_send_cmd("/arco/thru/new", 0, "iii", INPUT_ID, 1, ZERO_ID);
        o2_send_cmd("/arco/thru/new", 0, "iii", OUTPUT_ID, 2, ZERO_ID);
        o2_send_cmd("/arco/thru/new", 0, "iii", DUP_ID, 2, INPUT_ID);
        // o2_send_cmd("/arco/testtone/new", 0, "i", TESTTONE_ID);
        // let's put input in the output_set
        // o2_send_cmd("/arco/output", 0, "i", INPUT_ID);

        // o2_send_cmd("/arco/output", 0, "i", DUP_ID);

        o2_send_cmd("/arco/const/newf", 0, "if", FREQ_ID, 440.0f);
        o2_send_cmd("/arco/const/newf", 0, "if", AMP_ID, 0.1f);
        o2_send_cmd("/arco/sine/new", 0, "iiii", SINE_ID, 2, FREQ_ID, AMP_ID);
        o2_send_cmd("/arco/output", 0, "i", SINE_ID);

        auto_started = true;
        start_time = o2_time_get();
    } else if (!event1 && start_time + 2 < o2_time_get()) {
        event1 = true;
        o2_send_cmd("/arco/sine/set_freq", 0, "iif", SINE_ID, 0, 880.0f);
        o2_send_cmd("/arco/pwl/new", 0, "i", PWL_ID);
        o2_send_cmd("/arco/pwl/env", 0, "iffff", PWL_ID,
                    4410.0f, 1.0f, 44100.0f, 0.0f);
        o2_send_cmd("/arco/sine/repl_amp", 0, "ii", SINE_ID, PWL_ID);
        o2_send_cmd("/arco/pwl/start", 0, "i", PWL_ID);
        printf("event1!\n");
    } else if (!event2 && start_time + 4 < o2_time_get()) {
        event2 = true;
        o2_send_cmd("/arco/pwl/start", 0, "i", PWL_ID);
    }
}
#endif


// look up device name given a device ID
char *arco_name_lookup(int id)
{
    static char name[80];
    for (int i = 0; i < arco_device_info.size(); i++) {
        const char *info = arco_device_info[i].c_str();
        if (id == atoi(info)) {
            // found device, but we need to extrHact the name
            const char *start = strstr(info, " - ");
            start = strstr(start, " : ");
            const char *end = strrchr(info, '(');
            if (!start || !end) continue;  // just in case
            // compute final string length and check for overflow
            start += 3;
            end -= 1;
            if ((end - start) >= 80) continue;
            strncpy(name, start, end - start);
            name[end - start] = 0; // EOS
            return name;
        }
    }
    strcpy(name, "");
    return name;
}


void host_quit_audio()
{
    set_server_goal_state(FINISHED, "Cannot Quit now");
}


const char *aud_state_name[] = {
    "IDLE", "STARTING", "STARTED",
    "FIRST", "RUNNING", "STOPPING" };


/* O2 INTERFACE: /host/run int run;
   Run or stop audio processing.
*/
static void host_run(O2_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int run = argv[0]->i;
    // end unpack message
    if (run != 0 && server_aud_state == IDLE) {
        host_open_audio();
        printf("/host/run received. Starting audio devices.\n");
    } else if (run == 0 && server_aud_state == RUNNING) {
        printf("/host/run received. Closing audio devices.\n");
        host_close_audio();
    } else if (server_aud_state >= 0 && server_aud_state < 6) {
        printf("/host/run received. Ignored because state is %s\n",
               aud_state_name[server_aud_state]);
    } else {
        printf("/host/run received. Invalid server_aud_state: %d\n",
               server_aud_state);
    }
}


/* O2 INTERFACE: /host/clear;
   stop audio and remove any unit generators
*/
static void host_clear(O2_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    // end unpack message
    printf("/host/clear received. Closing reset in progress.\n");
    set_server_goal_state(RESET_IDLE, "Cannot clear now");
}


/* reset command is questionable since it could pull the rug out
from under a running application. On the other hand, maybe if an
application crashes, we want to do a reset and restart the
application. For now, we are removing reset support until we figure
out what it should do to support rather than screw users.

void host_reset_audio()
{
    if (server_aud_state == STARTING) {
        reset_requested = true;
        close_requested = true;
        return;
    }
    if (server_aud_state == RUNNING) {
        reset_requested = true;  // takes effect when aud_state becomes IDLE
        host_close_audio();
        return;
    }
    if (server_aud_state == STOPPING) {
        reset_requested = true;
        return;
    }
    assert(server_aud_state == IDLE);
    o2_send_cmd("/arco/ctrl", 0, "s", "host");
    o2_send_cmd("/arco/reset", 0, "");
}
*/

/* O2 not INTERFACE: /host/reset int32 status;
     Called when arco completes /arco/reset
 */
void host_reset(O2_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t status = argv[0]->i;
    // end unpack message

    if (status != 0) {
        arco_error("/host/reset failed with status %d.\n",
                   status);
        return;
    }
    o2_send_cmd("/actl/reset", 0, "i", status);
}



/* O2 INTERFACE: /host/starting
        int32 in_id, int32 out_id,
        int32 in_chans, int32 out_chans,
        int32 latency_ms, int32 buffer_size;
   Get actual parameters of audio stream.
*/
void host_starting(O2_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t in_id = argv[0]->i;
    int32_t out_id = argv[1]->i;
    int32_t in_chans = argv[2]->i;
    int32_t out_chans = argv[3]->i;
    int32_t latency_ms = argv[4]->i;
    int32_t buffer_size = argv[5]->i;
    // end unpack message

    host_in_id = in_id;
    host_out_id = out_id;
    host_in_chans = in_chans;
    host_out_chans = out_chans;
    host_latency_ms = latency_ms;
    host_buffer_size = buffer_size;
    
    /* printf("starting: in_id %d (%d chans), out_id %d (%d chans), "
           "latency %d ms, buffer_size %d\n", in_id, in_chans,
           out_id, out_chans, latency_ms, buffer_size); */

    if (server_aud_state != STARTING) {
        arco_warn("Got /host/starting but state is %d\n", server_aud_state);
    }
    if (host_in_chans == 0 && host_out_chans == 0) {
        server_goal_state = IDLE;  // if goal is running, changing current
            // state to IDLE will result in another /arco/open message,
            // which will probably fail again. Wait for user to change
            // device(s) or parameters before trying again.
        server_aud_state = IDLE;
        o2_send_cmd("/actl/started", 0, "i", 1);
    }
}

/* O2 INTERFACE: /host/started ;  -- notice when audio is started */
void host_started(O2_HANDLER_ARGS)
{
    if (server_aud_state != STARTING) {
        arco_warn("Got /host/starting but state is %d\n", server_aud_state);
    }
    // relay the started status to client
    o2_send_cmd("/actl/started", 0, "i", 0);
    server_aud_state = RUNNING;
}


/* O2 INTERFACE: /host/stopped int32 status;
*/
void host_stopped(O2_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t status = argv[0]->i;
    // end unpack message
    if (status < 0) {  // failed
        o2_sleep(2);
        server_aud_state = RUNNING;
        host_close_audio();
        return;
    }

    if (server_aud_state != STOPPING) {
        arco_warn("Got /host/stopped but state is %d\n", server_aud_state);
    }
    server_aud_state = IDLE;
}


static void host_initialize()
{
    o2_service_new("host");
    // O2 INTERFACE INITIALIZATION: (machine generated)
    o2_method_new("/host/devinf", "s", host_devinfo, NULL, true, true);
    o2_method_new("/host/starting", "iiiiii", host_starting, NULL, true, true);
    o2_method_new("/host/started", "", host_started, NULL, true, true);
    o2_method_new("/host/stopped", "i", host_stopped, NULL, true, true);
    o2_method_new("/host/reset", "i", host_reset, NULL, true, true);

    o2_method_new("/host/act", "iii", host_act, NULL, true, true);
    o2_method_new("/host/run", "i", host_run, NULL, true, true);
    o2_method_new("/host/clear", "", host_clear, NULL, true, true);
    // END INTERFACE INITIALIZATION
}


/********************* TEST FUNCTIONS *********************/

void test_tone()
{
    // frequency
    o2_send_cmd("/arco/const/newf", 0, "if", 4, 1000.0);
    // amplitude
    o2_send_cmd("/arco/const/newf", 0, "if", 5, 0.01);
    // sine tone(amplitude, frequency)
    o2_send_cmd("/arco/sine/new", 0, "iiii", 6, 1, 4, 5);
    // play it
    o2_send_cmd("/arco/output", 0, "i", 6);
}

#if defined(ARCO_CPU_MONITOR)
static void log_per_thread_cpu_times()
{
#if defined(__APPLE__)
    thread_act_array_t thread_list;
    mach_msg_type_number_t thread_count = 0;
    kern_return_t kr = task_threads(mach_task_self(), &thread_list,
                                    &thread_count);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "task_threads failed: %d\n", kr);
        return;
    }

    for (mach_msg_type_number_t i = 0; i < thread_count; i++) {
        thread_basic_info_data_t info;
        mach_msg_type_number_t count = THREAD_BASIC_INFO_COUNT;
        kr = thread_info(thread_list[i], THREAD_BASIC_INFO,
                         reinterpret_cast<thread_info_t>(&info), &count);
        if (kr != KERN_SUCCESS) {
            fprintf(stderr, "thread_info failed for thread %u: %d\n", i, kr);
            continue;
        }
        if ((info.flags & TH_FLAGS_IDLE) == 0) {
            double cpu_percent = (double) info.cpu_usage * 100.0 / 
                                 TH_USAGE_SCALE;
            double user_sec = info.user_time.seconds +
                              info.user_time.microseconds / 1e6;
            double sys_sec = info.system_time.seconds +
                             info.system_time.microseconds / 1e6;

            // Get a unique identifier for the thread
            thread_identifier_info_data_t idinfo;
            mach_msg_type_number_t idcount = THREAD_IDENTIFIER_INFO_COUNT;
            uint64_t mach_tid = 0;
            if (thread_info(thread_list[i], THREAD_IDENTIFIER_INFO,
                            reinterpret_cast<thread_info_t>(&idinfo),
                            &idcount) == KERN_SUCCESS) {
                mach_tid = idinfo.thread_id;
            }

            // Try to get a pthread name for this Mach thread
            char tname[64] = {0};
            pthread_t pthread = pthread_from_mach_thread_np(thread_list[i]);
            if (pthread) {
                pthread_getname_np(pthread, tname, sizeof(tname));
            }
            if (tname[0] != '\0') {
                printf("[CPU] %s (tid %llu): CPU %.2f%%, user %.3fs,"
                       " system %.3fs\n", tname, (unsigned long long) mach_tid,
                       cpu_percent, user_sec, sys_sec);
            } else {
                printf("[CPU] Thread %llu: CPU %.2f%%, user %.3fs,"
                       " system %.3fs\n", (unsigned long long) mach_tid, 
                       cpu_percent, user_sec, sys_sec);
            }
        }
    }

    for (mach_msg_type_number_t i = 0; i < thread_count; i++) {
        mach_port_deallocate(mach_task_self(), thread_list[i]);
    }
    vm_deallocate(mach_task_self(),
                  reinterpret_cast<vm_address_t>(thread_list),
                  thread_count * sizeof(thread_act_t));
#else
    // Not implemented on this platform
#endif
}
#endif

/********************* USER INTERFACE *********************/

vector<string> configuration_options;

void config_remove_midi_osc()
{
    current_config->remove_key("midi_in");
    current_config->remove_key("midi_out");
    current_config->remove_key("osc_in");
    current_config->remove_key("osc_out");
}

// At the completion of a configuration form (or when saving a form),
// find all the lines for MIDI and OSC services and put their data
// into the configuration.
 //
static void config_add_midi_osc()
{
    // first, clear all out existing entries for midi/osc in/out:
    config_remove_midi_osc();

    // search for all entries of interest in fields. After finding the first
    // field in a line, other fields are consecutive fields.
    for (int i = 0; i < tu->fields.size(); i++) {
        Field_entry *fe = tu->fields[i];
        if (fe->key == "osc_in_port") {
            vector<string> values;
            values.push_back(fe->content);
            fe = tu->fields[i + 1];
            assert(fe->key == "osc_in_tcp");
            values.push_back(fe->content);
            fe = tu->fields[i + 2];
            assert(fe->key == "osc_in_srv");
            values.push_back(fe->content);
            i += 3;  // remember there's an i++ at end of loop
            current_config->add("osc_in", Config::list_to_string(values));
       } else if (fe->key == "osc_out_srv") {
            vector<string> values;
            values.push_back(fe->content);
            fe = tu->fields[i + 1];
            assert(fe->key == "osc_out_ip");
            values.push_back(content_to_ip(fe->content));
            fe = tu->fields[i + 2];
            assert(fe->key == "osc_out_port");
            values.push_back(fe->content);
            fe = tu->fields[i + 3];
            assert(fe->key == "osc_out_tcp");
            values.push_back(fe->content);
            i += 4;  // remember there's an i++ at end of loop
            current_config->add("osc_out", Config::list_to_string(values));
         } else if (fe->key == "midi_in_dev") {
            vector<string> values;
            values.push_back(fe->content);
            fe = tu->fields[i + 1];
            assert(fe->key == "midi_in_srv");
            values.push_back(fe->content);
            i += 2;  // remember there's an i++ at end of loop
            current_config->add("midi_in", Config::list_to_string(values));
        } else if (fe->key == "midi_out_srv") {
            vector<string> values;
            values.push_back(fe->content);
            fe = tu->fields[i + 1];
            assert(fe->key == "midi_out_dev");
            values.push_back(fe->content);
            i += 2;  // remember there's an i++ at end of loop
            current_config->add("midi_out", Config::list_to_string(values));
        }
    }
}


static void config_callback(string key, int ch)
{
    if (key == "DIALOG_END") {
        if (ch == 0x13) {  // save fields to configuration
            configs.config("__configuration__").set_value("__configuration__",
                                  tu->get_string("configuration", "default"));
            current_config->set_value("arco_in_name",
                    arco_name_lookup(tu->get_int("arco_in_id", DFLT_IN_ID)));
            
            current_config->set_value("arco_out_name",
                    arco_name_lookup(tu->get_int("arco_out_id", DFLT_OUT_ID)));
            
            current_config->set_value("in_chans",
                    itos(tu->get_int("in_chans", DFLT_IN_CHANS)));
            
            current_config->set_value("out_chans",
                    itos(tu->get_int("out_chans", DFLT_OUT_CHANS)));
            
            current_config->set_value("buffer_size",
                    itos(tu->get_int("buffer_size", DFLT_BUFFER_SIZE)));
            
            current_config->set_value("latency",
                    itos(tu->get_int("latency", DFLT_LATENCY_MS)));
            
            current_config->set_value("network_option",
                    tu->get_string("network_option", DFLT_NETWORK_OPTION));
            
            current_config->set_value("o2lite_enable",
                    tu->get_bool("o2lite_enable", DFLT_O2LITE_ENABLE) ?
                     "T" : "F");
            
            current_config->set_value("debug_flags",
                    tu->get_string("debug_flags", DFLT_DEBUG_FLAGS));
            
            current_config->set_value("ensemble",
                    tu->get_string("ensemble", DFLT_ENSEMBLE_NAME));
            
            current_config->set_value("polling_rate",
                    itos(tu->get_int("polling_rate", DFLT_POLLING_RATE)));
            
            current_config->set_value("reference_clock",
                    tu->get_bool("reference_clock", DFLT_REFERENCE_CLOCK) ?
                    "T" : "F");
            
            current_config->set_value("http_root",
                    tu->get_string("http_root", DFLT_HTTP_ROOT));
            
            current_config->set_value("http_port",
                    itos(tu->get_int("http_port", DFLT_HTTP_PORT)));
            
            current_config->set_value("mqtt_host",
                    tu->get_string("mqtt_host", DFLT_MQTT_HOST));
            
            current_config->set_value("mqtt_port",
                    itos(tu->get_int("mqtt_port", DFLT_MQTT_PORT)));
            
            printf("*** Save (P) to keep any preference changes for"
                   " next time\n");
            
            if (config_to_prefs()) {
                printf("*** Quit (Q) and restart for network\n"
                       "***   preference changes to take effect.\n");
            }
            
            // now process the midi/osc in/out lines
            config_add_midi_osc();
            
            configuration_options.clear();
        }
        tu->fixed_info(&top_lines, &bottom_lines);
    } else if (key == "configuration") {
        current_config = &configs.config(tu->get_string("configuration",
                                                        "default"));
        dialog_configure();
    } else if (key == "config_save" || key == "config_new") {
        // Copy and save the current configuration under a new name
        int new_name_i = tu->find_field("name");
        string new_name = tu->fields[new_name_i]->content;
        if (key == "config_save") {
            new_name = configs.save_copy_as(current_config, new_name);
        } else {  // key == "config_new
            new_name = configs.new_config(new_name);
        }
        configuration_options.push_back(new_name);
        int fi = tu->find_field("configuration");
        assert(fi >= 0);
        tu->fields[fi]->set_content(new_name);
        tu->fields[fi]->show_content(tu);
        current_config = &configs.config(new_name);
        if (key == "config_new") {  // configuration changed, reload
            dialog_configure();
        }
    } else if (key == "restore") {
        prefs_to_config();
        dialog_configure();
    } else if (key == "delete") {
        string name = current_config->get_string_value("name");
        configs.remove(name);
        int loc = string_vector_index(configuration_options, name, -1);
        assert(loc >= 0);
        configuration_options.erase(configuration_options.begin() + loc);
        // update the field to a configuration and switch to it
        int fi = tu->find_field("configuration");
        assert(fi >= 0);
        tu->fields[fi]->set_option(tu, 0, 0);
        // set_option invokes this callback recursively with
        // key "configuration", which sets up a new configuration
        // since the field key is "configuration", the callback
        // (this function) will ignore the 3rd parameter (0).
#if USE_MIDI
    } else if (key == "midi_in_new") {
        insert_midi_to_o2();
    } else if (key == "midi_out_new") {
        insert_o2_to_midi();
#endif
    } else if (key == "osc_to_o2_new") {
        insert_osc_to_o2();
    } else if (key == "o2_to_osc_new") {
        insert_o2_to_osc();
    } else if (key == "midi_in_del" || key == "midi_out_del" ||
               key == "osc_in_del" || key == "osc_out_del") {
        tu->dialog_remove_line(tu->dialog_y);
        midi_osc_line_count--;
        if (midi_osc_line_count == 0) {
            tu->dialog_remove_line(tu->dialog_y);
        }
        tu->dialog_refresh();
#if USE_MIDI
    } else if (key == "midi_refresh") {
        midi_devices_refresh();
#endif
    }
}

void create_midi_handlers()
{
#if USE_MIDI
    midi_services_finish();  // clean up any existing connections

    vector<string> values = current_config->get_values("midi_in");
    for (string &value : values) {
        vector<string> contents = Config::string_to_list(value);
        if (contents.size() != 2) {
            printf("Bad midi_in configuration value: %s\n", value.c_str());
            continue;
        }
        midi_input_initialize(contents[0].c_str(), contents[1]);
    }

    values = current_config->get_values("midi_out");
    for (string &value : values) {
        vector<string> contents = Config::string_to_list(value);
        if (contents.size() != 2) {
            printf("Bad midi_in configuration value: %s\n", value.c_str());
            continue;
        }
        midi_output_initialize(contents[0], contents[1].c_str());
    }
#endif
}


// Given current_config, create handlers for MIDI and OSC services
//
void create_midi_osc_handlers()
{
    osc_input_output_finish();  // clean up any existing connections

    // separate function also used by midi_devices_refresh
    create_midi_handlers();

    vector<string> values = current_config->get_values("osc_in");
    for (string &value : values) {
        vector<string> contents = Config::string_to_list(value);
        if (contents.size() != 3) {
            printf("Bad midi_in configuration value: %s\n", value.c_str());
            continue;
        }
        osc_input_initialize(contents[2].c_str(), atoi(contents[0].c_str()),
                             contents[1] == "TCP");
    }

    values = current_config->get_values("osc_out");
    for (string &value : values) {
        vector<string> contents = Config::string_to_list(value);
        if (contents.size() != 4) {
            printf("Bad midi_in configuration value: %s\n", value.c_str());
            continue;
        }
        osc_output_initialize(contents[0].c_str(), contents[1].c_str(),
                              atoi(contents[2].c_str()), contents[3] == "TCP");
    }
}


// insert a blank line before key "arco_in_id" where we can insert
// fields to map between MIDI or OSC and O2
int open_midi_osc_line()
{
    // find "arco_in_id" -- that's the line we want to use
    int in_index = tu->find_field("arco_in_id");
    assert(in_index >= 0);
    int y = tu->fields[in_index]->y;  // y is our line
    tu->dialog_insert_line(y);
    if (midi_osc_line_count == 0) {
        tu->field_blank("before_forwarding", y);  // need a separator line
    } else {
        y--;  // place y above blank line above "Input devices:" line
    }
    midi_osc_line_count++;
    return y;
}


int dialog_midi_osc_setup(int ln)
{
    // iterate through configuration (current_config) and create
    // fields for every midi_in, midi_out, osc_in, osc_out element
    // sort the elements into the above order

    vector<string> values;
#if USE_MIDI
    values = current_config->get_values("midi_in");
    for (string &value : values) {
        vector<string> contents = Config::string_to_list(value);
        if (contents.size() != 2) {
            printf("Bad midi_in configuration value: %s\n", value.c_str());
            continue;
        }
        insert_midi_to_o2_fields(ln++, contents[0], contents[1]);
        midi_osc_line_count++;
    }

    values = current_config->get_values("midi_out");
    for (string &value : values) {
        vector<string> contents = Config::string_to_list(value);
        if (contents.size() != 2) {
            printf("Bad midi_in configuration value: %s\n", value.c_str());
            continue;
        }
        insert_o2_to_midi_fields(ln++, contents[0], contents[1]);
        midi_osc_line_count++;
    }
#endif

    values = current_config->get_values("osc_in");
    for (string &value : values) {
        vector<string> contents = Config::string_to_list(value);
        if (contents.size() != 3) {
            printf("Bad midi_in configuration value: %s\n", value.c_str());
            continue;
        }
        insert_osc_to_o2_fields(ln++, contents[0], contents[1], contents[2]);
        midi_osc_line_count++;
    }

    values = current_config->get_values("osc_out");
    for (string &value : values) {
        vector<string> contents = Config::string_to_list(value);
        if (contents.size() != 4) {
            printf("Bad midi_in configuration value: %s\n", value.c_str());
            continue;
        }
        insert_o2_to_osc_fields(ln++, contents[0], contents[1],
                                contents[2], contents[3]);
        midi_osc_line_count++;
    }


    return ln + (midi_osc_line_count > 0);
}


// dialog_configure - build form from configuration and start dialog
void dialog_configure()
{
    tu->fixed_info(&top_lines, &dialog_bottom_lines);
    tu->dialog_begin();
    // location parameters are: x, y, w1, w2, where x = 0 means left column,
    // x > 0 means advance this many spaces, y = 0 means same line,
    // y > 0 means advance this many lines (typically 1), w1 is width of
    // label, which is padded on the left with blanks, w2 is width of the
    // data entry field

    midi_osc_line_count = 0;
    configuration_options = configs.get_configuration_names();
    
    int ln = tu->top_size();
    tu->field_menu("Configuration:", "configuration",
                    current_config->get_string_value("name"),
                   0, ln, 18, 26,  configuration_options);
    tu->field_button("Restore:", "config_restore", 49, ln, 8, 1);
    tu->field_button("Delete:", "config_delete", 61, ln++, 7, 1);
    tu->field_string("New Configuration:", "name", "", 0, ln, 18, 26);
    tu->field_button("Copy This:", "config_save", 49, ln, 10, 1);
    tu->field_button("New", "config_new", 63, ln++, 3, 1);
    tu->field_blank("after_config_new", ln++);

    tu->field_string("Ensemble name:", "ensemble",
                     current_config->get_string_value("ensemble"),
                     0, ln, 18, 32);
    tu->field_int("Polling rate:", "polling_rate",
                  current_config->get_string_value("polling_rate"),
                  1, 1000, 54, ln++, 13, 4);
    tu->field_string("Debug flags:", "debug_flags", // prefs_debug_flags(),
                     current_config->get_string_value("debug_flags"),
                     0, ln, 18, 32);
    tu->field_bool("Ref Clock:", "reference_clock",
                   current_config->get_string_value("reference_clock"),
                   54, ln++, 14, 3);
    tu->field_menu("Networking:", "network_option", // prefs_network_option(),
                   current_config->get_string_value("network_option"),
                   0, ln, 18, 19, net_options);
    tu->field_bool("O2lite enable:", "o2lite_enable",
                   // prefs_o2lite_enable ? "T" : "F",
                   current_config->get_string_value("o2lite_enable"),
                   54, ln++, 14, 3);
    tu->field_int("HTTP Port:", "http_port",
                  current_config->get_string_value("http_port"),
                  // itos(prefs_http_port()),
                  0, 65535, 0, ln, 10, 5);
    tu->field_string("Root:", "http_root",
                     // prefs_http_root(),
                     current_config->get_string_value("http_root"),
                     17, ln++, 5, 49);
    tu->field_string("MQTT Host:", "mqtt_host",
                     // prefs_mqtt_host(),
                     current_config->get_string_value("mqtt_host"),
                     0, ln, 10, 42);
    tu->field_int("HTTP Port:", "mqtt_port", // itos(prefs_mqtt_port()),
                  current_config->get_string_value("mqtt_port"),
                  0, 65535, 54, ln++, 12, 5);
    tu->field_button("New forward O2 to OSC:", "o2_to_osc_new",
                     0, ln, 22, 1);
    tu->field_button("New forward OSC to O2:", "osc_to_o2_new",
                     32, ln++, 22, 1);
#if USE_MIDI
    tu->field_button("New MIDI In to O2:", "midi_in_new",
                     0, ln, 18, 1);
    tu->field_button("New MIDI Out from O2:", "midi_out_new",
                     25, ln, 21, 1);
    tu->field_button("MIDI Refresh:", "midi_refresh",
                     54, ln++, 13, 1);
#endif
    tu->field_blank("after_server_prefs", ln++);

    // are there midi/osc in/out lines to configure?
    ln = dialog_midi_osc_setup(ln);

    // Now add the audio configuration fields:
    string in_name = current_config->get_string_value("arco_in_name");
    tu->field_int("Input device:", "arco_in_id",
                  itos(lookup_device_id(in_name)),
                  0, 99, 0, ln, 17, 2);
    string out_name = current_config->get_string_value("arco_out_name");
    tu->field_int("Output device:", "arco_out_id",
                  itos(lookup_device_id(out_name)),
                  0, 99, 32, ln++, 17, 2);
    tu->field_int("Input channels:", "in_chans",
                  current_config->get_string_value("in_chans"),
                  0, 64, 0, ln, 17, 2);
    tu->field_int("Output channels:", "out_chans",
                  current_config->get_string_value("out_chans"),
                  0, 64, 32, ln++, 17, 2);
    tu->field_int("Buffer size:", "buffer_size",
                  current_config->get_string_value("buffer_size"),
                  1, 9999, 0, ln, 17, 4);
    tu->field_int("Latency:", "latency",
                  current_config->get_string_value("latency"),
                  0, 299, 32, ln++, 17, 3);
    
    move(ln, 0);
    hline(ACS_HLINE, 72);
    ln++;

    // show the audio devices
    for (int j = 0; j < arco_device_info.size(); j++) {
        mvaddstr(ln++, 0, "   ");
        addstr(arco_device_info[j].c_str());
        clrtoeol();
    }
    mvaddstr(ln++, 0, "   CTRL-S to save and exit. Leave blanks for defaults.");
    clrtoeol();
    mvaddstr(ln++, 0, "   ESC exits without changes.");
    clrtoeol();
    move(ln++, 0);
    clrtoeol();  // blank line after instructions
    tu->set_field_callback(&config_callback);

    // clear remaining lines to bottom_info:
    int x, y;
    getmaxyx(stdscr, y, x);
    y -= tu->bottom_size();
    while (ln < y) {
        move(ln++, 0);
        clrtoeol();
    }
    tu->dialog_run();
}

/*
void configure_screen_finish()
{
    // update preferences
    prefs_set_in_name(arco_name_lookup(arco_in_id));
    p_in_id = arco_in_id;
    prefs_set_out_name(arco_name_lookup(arco_out_id));
    p_out_id = arco_out_id;
    prefs_set_in_chans(arco_in_chans);
    prefs_set_out_chans(arco_out_chans);
    prefs_set_buffer_size(arco_buffer_size);
    prefs_set_latency(arco_latency_ms);
    prefs_set_network_option(arco_network_option);
    prefs_set_o2lite_enable(arco_o2lite_enable);
    prefs_set_debug_flags(arco_debug_flags);
    if (strcmp(host_network_option, prefs_network_option()) != 0 ||
        host_o2lite_enable != prefs_o2lite_enable()) {
        printf("*** Save (P), Quit (Q), and restart for network\n"
               "    preference changes to take effect.\n");
    }
    if (server_set_debug_flags(arco_debug_flags)) {
        o2_debug_flags(prefs_debug_flags());
        printf("*** Set new debug flags: \"%s\".\n", prefs_debug_flags());
    }
}
*/

// Most commands are shared between curses and tty interfaces
// and handled here:
void action(int ch, bool is_escape)
{
    if (is_escape) {
        return;  // no special actions for escape characters
    }
    if (!arco_ready) {
        printf("Ignored input. Arco has not started.\n");
        return;  // Don't interact with Arco before it is ready.
    }
    switch (ch) {
      case 'A': // configuration dialog
        dialog_configure();
        break;
      case 'B':  // "heartbeat" block counts on/off
        heartbeat_enabled = !heartbeat_enabled;
        o2_send_cmd("/arco/hb", 0, "i", heartbeat_enabled);
        break;
#if defined(ARCO_CPU_MONITOR)
      case 'C':  // toggle CPU monitor
        cpu_monitor_enabled = !cpu_monitor_enabled;
        if (cpu_monitor_enabled) {
            cpu_monitor_last_log = o2_local_time();
            printf("CPU monitor enabled (interval %.1fs).\n", cpu_monitor_interval);
        } else {
            printf("CPU monitor disabled.\n");
        }
        break;
#endif
      case 'Q':
        host_quit_audio();
        break;
//      case 'R':  // reset: remove all unit generators
//        host_reset_audio();
//        break;
      case 'S':  // start/stop
        if (server_aud_state == RUNNING) {
            printf("Closing audio devices.\n");
            host_close_audio();
        } else if (server_aud_state == IDLE) {
            host_open_audio();
        } else {
            printf("Start/stop ignored because state is not"
                   " IDLE or RUNNING.\n");
        }
        break;
      case 'P':  // save parameters
        prefs_write();
        break;
      case 'T':
        test_tone();
        break;
      case 'U':  // print the audio tree
        o2_send_cmd("/arco/prtree", 0, "");
        break;
      default: {
        char quoted[16];
        const char *inp = quoted;
        if (ch == '\n') inp = "RETURN";
        else if (ch == '\t') inp = "TAB";
        else if (ch == '\r') inp = "CR";
        else if (ch == ' ') inp = "SPACE";
        else if (ch == 27) inp = "ESC";
        else if (ch == 127) inp = "DELETE";
        else if (ch == (ch & 0x1F)) {  // CTRL- key
            snprintf(quoted, 16, "CTRL-%c", ch + 0x40);
        } else if (std::isprint(ch)) {
            quoted[0] = '\'';
            quoted[1] = ch;
            quoted[2] = '\'';
            quoted[3] = 0;
        } else {
            snprintf(quoted, 16, "'\\x%02x'", ch);
        }
        printf("%s is not a key command.\n", inp);
        break;
      }
    }
}


void print_help(char *cmd)
{
    printf("usage: %s [starting_configuration]\n", cmd);
    printf("    where starting_configuration is a configuration name\n");
    printf("    from the configuration file arco_server_prefs.json\n");
}


int main(int argc, char *argv[])
{
    const char *default_configuration = nullptr;
    if (argc > 2) {
        print_help(argv[0]);
        exit(1);
    } else if (argc == 2) {  // parameter is configuration to use
        default_configuration = argv[1];
    }

    o2_use_logfile(true);  // divert debug info because of console UI

    // ahprintf("main: initial latency %d\n", prefs_latency_ms());
    tu = new Terminal_ui(200);
    tu->set_key_callback(&action);
    if (tu->uiscr == nullptr) {
        exit(1);
    }
    tu->help_keys("H");
    tu->dialog_escape_keys("\x13\x18\x1b");  // CTRL-S, CTRL-X, ESC
    tu->add_help("A - set Audio preferences");
    tu->add_help("B - block count messages ON/OFF");
#if defined(ARCO_CPU_MONITOR)
    tu->add_help("C - Toggle CPU monitor");
#endif
    tu->add_help("H - describe commands like this");
    tu->add_help("P - Save latest selections to preferences");
    tu->add_help("Q - Quit the program");
    tu->add_help("R - Reset server: deletes all unit generators");
    tu->add_help("S - Start or Stop");
    tu->add_help("t - ask audio thread to print a test message");
    tu->add_help("T - test tone");
    tu->add_help("U - Print audio ugen tree");

    dialog_bottom_lines.push_back(string("Press ^S to save and exit, ESC to cancel"));
    top_lines.push_back(string("Arco v4"));
#ifdef ARCO_CPU_MONITOR
    bottom_lines.push_back(string("(A)Configure (S)tart/Stop (R)eset (C)PU (Q)uit (H)elp"));
#else
    bottom_lines.push_back(string("(A)Configure (S)tart/Stop (R)eset (Q)uit (H)elp"));
#endif

    tu->fixed_info(&top_lines, &bottom_lines);  // one info line at top, one at bottom
    
    load_configurations(default_configuration);
    config_to_prefs();

    // must be true for O2lite/Zeroconf discovery:
    strncpy(host_network_option, prefs_network_option(),
            sizeof(host_network_option));
    host_network_option[sizeof(host_network_option) - 1] = 0;
    string hno {host_network_option};
    int opt = string_vector_index(net_options, hno, 0);
    o2_network_enable(opt > 0);

    o2_internet_enable(opt > 1);

    o2_initialize(prefs_ensemble_name());
    // o2_clock_set is called by audio_initialize

    host_o2lite_enable = prefs_o2lite_enable();
    if (host_o2lite_enable) {
        o2lite_initialize();  // enable O2lite client connections
    }
    
    host_http_enable = prefs_http_enable();
    host_http_port = prefs_http_port();
    strncpy(host_http_root, prefs_http_root(),
            sizeof(host_http_root));
    host_http_root[sizeof(host_http_root) - 1] = 0;
    if (host_http_enable) {
        o2_http_initialize(host_http_port, host_http_root);
    }

    if (opt > 2) {
        o2_mqtt_enable(NULL, 0);  // only default MQTT server supported now
    }

    host_initialize();  // set up handlers

    create_midi_osc_handlers();

    int err;
    if ((err = arco_initialize())) {
        tu->finish();
        printf("FATAL ERROR %d in arco_initiailize. Exiting now.\n", err);
        exit(1);
    }
    

    int running = true;
    int shared_mem_active = true;
    int poll_count = 0;
    double start_timing = o2_local_time();
    pthread_setname_np("arco-server-host");
    while (server_aud_state != FINISHED) {
        tu->poll(1000000 / prefs_polling_rate());  // 2 ms polling period
#if USE_MIDI
        midi_poll();
#endif
#if defined(ARCO_CPU_MONITOR)
        if (cpu_monitor_enabled) {
            double now = o2_local_time();
            if (now - cpu_monitor_last_log >= cpu_monitor_interval) {
                printf("--- CPU per-thread snapshot ---\n");
                log_per_thread_cpu_times();
                printf("est pollling rate (Hz): %g\n",
                       poll_count / (now - cpu_monitor_last_log));
                poll_count = 0;
                cpu_monitor_last_log = now;
            }
        }
#endif
        if (server_goal_state != server_aud_state) {
            // if goal is RUNNNING, either we're IDLE and need to
            // start, or we've started, state is STARTING, and action pending
            if (server_goal_state == RUNNING) {
                if (server_aud_state == IDLE) {
                    server_aud_state = STARTING;
                    // expecting /host/starting:
                    o2_send_cmd("/arco/open", 0, "iiiiii", p_in_id, p_out_id,
                                prefs_in_chans(), prefs_out_chans(),
                                prefs_latency_ms(), prefs_buffer_size());
                }
            } else if ((server_goal_state == IDLE ||
                        server_goal_state == RESET_IDLE ||
                        server_goal_state == FINISHED) &&
                       (server_aud_state == RUNNING)) {
                server_aud_state = STOPPING; 
               // expecting /host/starting:
                o2_send_cmd("/arco/ctrl", 0, "s", "host");
                o2_send_cmd("/arco/close", 0, "");
            } else if (server_aud_state == FINISH1) {
                if (o2_shmem_inst_count() == 0) {
                    server_aud_state = FINISHED;
                }
            } else if (server_goal_state == FINISHED &&
                       server_aud_state == IDLE) {
                server_aud_state = FINISH1;
                o2_send_cmd("/arco/quit", 0, "");
            } else if (server_goal_state == RESET_IDLE &&
                       server_aud_state == IDLE) {
                o2_send_cmd("/arco/reset", 0, "");
                server_goal_state = IDLE;  // clear the reset request
            }
            double wait = o2_local_time() - server_wait_since;
            if (wait > 5) {
                arco_warn("current state: %d, goal state %d, elapsed time: %g",
                          server_aud_state, server_goal_state, wait);
            }
        }

        O2err err = o2_poll();
        if (err == O2_INTERRUPT_REQUESTED) {
            o2_reset_interrupt_request();
        }
        arco_thread_poll();
        if (!arco_ready) {
            arco_ready = (o2_status("arco") == O2_BRIDGE);
            // one-shot setup when arco thread is ready to communicate:
            if (arco_ready) {
                o2_send_cmd("/arco/ctrl", 0, "s", "host");
                if (!has_curses) {  // turn block count messages off
                    o2_send_cmd("/arco/hb", 0, "i", 0);
                }
                // reset preference device id's which may have changed
                // /host/devinf callbacks will attempt to reassign them
                // to match the preference device names
                p_out_id = -1;
                p_in_id = -1;
                o2_send_cmd("/arco/devinf", 0, "s", "/host/devinf");
            }
        }
        poll_count++;
    }
    o2_bridges_finish();
    tu->finish();
    arco_device_info.clear();
    o2_finish();
    return 0;
}

