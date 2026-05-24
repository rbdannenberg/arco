/* arco.cpp -- main program for arco server
 *
 * Roger B. Dannenberg
 * Dec 2021
 */

#include "o2internal.h"  // need internal to offer bridge
#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#ifdef __linux__
#include "ncurses.h"
#else
#include "curses.h"
#endif
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include "o2atomic.h"
#include "sharedmem.h"   // o2_shmem_inst_new()
#include "arcotypes.h"
#include "arcoinit.h"
#include "arco_ui.h"
#include "ugenids.h"
#include "audioio.h"
#include "prefs.h"
#include "svprefs.h"
#include "arcoutil.h"
/*
#include "ugen.h"
#include "zero.h"
#include "zerob.h"
#include "thru.h"
*/

// User preferences edited by the Configuration dialog. These are
// initialized to preferences, but may be edited. These are the
// values that are actually used:
int arco_in_id;
int arco_out_id;
int arco_in_chans;
int arco_out_chans;
int arco_buffer_size;
int arco_latency_ms;
bool arco_network_enable;
bool arco_o2lite_enable;
bool arco_internet_enable;
bool arco_mqtt_enable;

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
static bool host_network_enable = true;
static bool host_o2lite_enable = true;
static bool host_internet_enable = true;
static bool host_mqtt_enable = false;

// leading 'c' means curses only, 'p' means plain terminal only

const char *help_strings[] = {
    " A - set Audio preferences",
    " B - block count messages ON",
    " b - block count messages OFF",
    " H - describe commands like this",
    " P - Save latest selections to preferences",
    " p - Print audio ugen tree",
    " Q - Quit the program",
    " R - Reset server: deletes all unit generators",
    " S - Start or Stop",
    " t - ask audio thread to print a test message",
    " T - test tone",
    NULL  // must terminate this list with NULL
};

const char *main_commands = "(A)udio device (S)tart/Stop (R)eset (Q)uit (H)elp";

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
    

/* O2 INTERFACE: /host/openaggr string info;
    For rapid setup and debugging, this looks for the aggregate device
    and when found, opens it.
*/
void host_openaggr(O2_HANDLER_ARGS)
{
    const int LATENCY_MS = 50;

    // begin unpack message (machine-generated):
    char *info = argv[0]->s;
    // end unpack message

    printf("%s\n", info);

    if (strstr(info, "Aggregate")) {
        int id = atoi(info);
        printf("  --> opening this device, id %d\n", id);
        // open audio, 50ms latency is conservative
        o2_send_cmd("/arco/ctrl", 0, "s", "host");
        o2_send_cmd("/arco/open", 0, "iiiiii", id, id, 
                    prefs_in_chans(), prefs_out_chans(), LATENCY_MS, BL);
        o2_send_cmd("/arco/prtree", o2_time_get() + 1, "");
        /* Log some audio:
        o2_send_cmd("/arco/logaud", 0, "iis", 1, 1, "log.wav");
        o2_send_cmd("/arco/logaud", o2_time_get() + 1, "iis", 0, 1, "");
        */
    }
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

        // get audio devices
        o2_send_cmd("/arco/devinf", 0, "s", "/host/openaggr");
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


void host_ugens_init()
{
    // Note: channels here are based on preferences. We try to open 
    // matching counts, but if devices cannot achieve the preferences,
    // inputs are expanded to the preferred count and outputs are mixed
    // down to the available output channels.
    arco_print("**** host_ugens_init initializing %d ins %d outs ****\n",
               prefs_in_chans(), prefs_out_chans());
    assert(prefs_in_chans() >= 0);
    assert(prefs_out_chans() >= 0);
    o2_send_cmd("/arco/zero/new", 0, "i", ZERO_ID);
    o2_send_cmd("/arco/zerob/new", 0, "i", ZEROB_ID);
    o2_send_cmd("/arco/thru/new", 0, "iii", INPUT_ID,
                prefs_in_chans(), ZERO_ID);
    o2_send_cmd("/arco/thru/new", 0, "iii", OUTPUT_ID, 
                prefs_out_chans(), INPUT_ID);
    server_aud_is_reset = true;
}


int main(int argc, char *argv[])
{
    prefs_read();

    printf("main: initial latency %d\n", prefs_latency_ms());
    has_curses = ui_init(200) >= 0;  // <0 means error, no curses interface
    // must be true for O2lite/Zeroconf discovery:
    host_network_enable = prefs_network_enable();
    o2_network_enable(host_network_enable);

    host_o2lite_enable = prefs_o2lite_enable();
    if (host_o2lite_enable) {
        o2lite_initialize();  // enable O2lite client connections
    }

    host_internet_enable = prefs_internet_enable();
    o2_internet_enable(host_internet_enable);

    o2_debug_flags("");
    o2_initialize("arco");
    o2_clock_set(NULL, NULL);  // we are the reference clock

    host_mqtt_enable = prefs_mqtt_enable();
    if (host_mqtt_enable) {
        o2_mqtt_enable(NULL, 0);  // only default MQTT server supported now
    }

    host_initialize();  // set up handlers
    int err;
    if ((err = arco_initialize())) {
        printf("FATAL ERROR %d in arco_initiailize. Exiting now.\n", err);
        exit(1);
    }

    int running = true;
    int shared_mem_active = true;
    while (server_aud_state != FINISHED) {
        ui_poll(2);  // 2 ms polling period
        if (server_goal_state != server_aud_state) {
            // if goal is RUNNNING, either we're IDLE and need to
            // start, or we've started, state is STARTING, and action pending
            if (server_goal_state == RUNNING) {
                if (server_aud_state == IDLE) {
                    host_ugens_init();
                    server_aud_state = STARTING;
                    // expecting /host/starting:
                    o2_send_cmd("/arco/open", 0, "iiiiii", p_in_id, p_out_id,
                                prefs_in_chans(), prefs_out_chans(),
                                prefs_latency_ms(), prefs_buffer_size());
                }
            } else if ((server_goal_state == IDLE ||
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
            }

            double wait = o2_local_time() - server_wait_since;
            if (wait > 5) {
                arco_warn("current state: %d, goal state %d, elapsed time: %g",
                          server_aud_state, server_goal_state, wait);
            }
        }

        o2_poll();
        arco_thread_poll();
        if (!arco_ready) {
            arco_ready = (o2_status("arco") == O2_BRIDGE);
            // one-shot setup when arco thread is ready to communicate:
            if (arco_ready) {
                o2_send_cmd("/arco/ctrl", 0, "s", "host");
                if (!has_curses) {  // turn block count messages off
                    o2_send_cmd("/arco/hb", 0, "i", 0);
                }
                o2_send_cmd("/arco/devinf", 0, "s", "/host/devinf");
            }
        }
        // automation();
        o2_sleep(2);
    }
    o2_bridges_finish();
    ui_finish();
    
    for (int i = 0; i < arco_device_info.size(); i++) {
        O2_FREE((void *) arco_device_info[i]);
    }
    arco_device_info.finish();
    dminfo.finish();
    
    o2_finish();
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
    arco_device_info.push_back(o2_heapify(info));
}


void host_close_audio()
{
    return set_server_goal_state(IDLE, "Cannot Close now");
}


/* start audio processing */
int host_open_audio()
{
    return set_server_goal_state(RUNNING, "Cannot Open now");
}


void host_quit_audio()
{
    return set_server_goal_state(FINISHED, "Cannot Quit now");
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
(this is the old handler for Arco's /host/reset message; it is
disabled until we figure out what's best to support applications. See
audioio.cpp under "Reset: More State" and see host_ugens_init().)
void host_reset(O2_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t status = argv[0]->i;
    // end unpack message

    if (status != 0) {
        arco_error("/host/reset with status %d ignored because of status\n",
                   status);
        return;
    }

    // Note: channels here are based on preferences. We try to open 
    // matching counts, but if devices cannot achieve the preferences,
    // inputs are expanded to the preferred count and outputs are mixed
    // down to the available output channels.
    arco_print("**** audio was reset, initializing %d ins %d outs ****\n",
               prefs_in_chans(), prefs_out_chans());
    assert(prefs_in_chans() >= 0);
    assert(prefs_out_chans() >= 0);
    o2_send_cmd("/arco/zero/new", 0, "i", ZERO_ID);
    o2_send_cmd("/arco/zerob/new", 0, "i", ZEROB_ID);
    o2_send_cmd("/arco/thru/new", 0, "iii", INPUT_ID,
                prefs_in_chans(), ZERO_ID);
    o2_send_cmd("/arco/thru/new", 0, "iii", OUTPUT_ID, 
                prefs_out_chans(), INPUT_ID);
    server_aud_state = IDLE;
    server_aud_is_reset = true;

    if (open_requested) {
        open_requested = false;
        // reset happens when we first start audio, so now we complete
        // starting audio by opening the audio stream:
        host_open_audio();
    }
}
*/


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
        server_aud_state = IDLE;
    }
}

/* O2 INTERFACE: /host/started ;  -- notice when audio is started */
void host_started(O2_HANDLER_ARGS)
{
    if (server_aud_state != STARTING) {
        arco_warn("Got /host/starting but state is %d\n", server_aud_state);
    }
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


// look up device name given a device ID
char *arco_name_lookup(int id)
{
    static char name[80];
    for (int i = 0; i < arco_device_info.size(); i++) {
        const char *info = arco_device_info[i];
        if (id == atoi(info)) {
            // found device, but we need to extrHact the name
            const char *start = strstr(info, " - ");
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


static void host_initialize()
{
    o2_service_new("host");
    // O2 INTERFACE INITIALIZATION: (machine generated)
    o2_method_new("/host/devinf", "s", host_devinfo, NULL, true, true);
    o2_method_new("/host/openaggr", "s", host_openaggr, NULL, true, true);
    o2_method_new("/host/starting", "iiiiii", host_starting, NULL, true, true);
    o2_method_new("/host/started", "", host_started, NULL, true, true);
    o2_method_new("/host/stopped", "i", host_stopped, NULL, true, true);
//    o2_method_new("/host/reset", "i", host_reset, NULL, true, true);
    // END INTERFACE INITIALIZATION
}


bool arco_started = false;
bool arco_reset = false;    // helps implement one-time reset. To reset again,
                            // either a client sends reset, or restart server.


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

/********************* USER INTERFACE *********************/

void config_dialog()
{
    // look up device numbers in preferences
    for (int i = 0; i < arco_device_info.size(); i++) {
        const char *info = arco_device_info[i];
        int id = atoi(info);
        p_in_id = prefs_in_lookup(info, id);
        p_out_id = prefs_out_lookup(info, id);
    }
    arco_in_id = p_in_id;
    arco_out_id = p_out_id;
    arco_in_chans = p_in_chans;
    arco_out_chans = p_out_chans;
    arco_buffer_size = prefs_buffer_size();
    arco_latency_ms = prefs_latency_ms();
    arco_network_enable = prefs_network_enable();
    arco_o2lite_enable = prefs_o2lite_enable();
    arco_internet_enable = prefs_internet_enable();
    arco_mqtt_enable = prefs_mqtt_enable();

    ui_start_dialog();
    // "Current" values are displayed as information to user from
    // host_* variables unless there is no actual value yet,
    // in which case we assume the preferences will give us a value:
    ui_int_field("Input device:   ", &arco_in_id, -1, 100,
                 host_in_id, p_in_id);
    ui_int_field("Output device:  ", &arco_out_id, -1, 100,
                 host_out_id, p_out_id);
    ui_int_field("Input channels: ", &arco_in_chans, -1, 100,
                 host_in_chans, prefs_in_chans());
    ui_int_field("Output channels:", &arco_out_chans, -1, 100,
                 host_out_chans, prefs_out_chans());
    ui_int_field("Buffer size:    ", &arco_buffer_size, -1, 1024,
                 host_buffer_size, prefs_buffer_size());
    ui_int_field("Latency:        ", &arco_latency_ms, -1, 1024,
                 host_latency_ms, prefs_latency_ms());
    ui_bool_field("Network enable: ", &arco_network_enable,
                  host_network_enable, prefs_network_enable());
    ui_bool_field("O2lite enable:  ", &arco_o2lite_enable,
                  host_o2lite_enable, prefs_o2lite_enable());
    ui_bool_field("Internet enable:", &arco_internet_enable,
                  host_internet_enable, prefs_internet_enable());
    ui_bool_field("MQTT enable:    ", &arco_mqtt_enable,
                  host_mqtt_enable, prefs_mqtt_enable());
    ui_run_dialog("Arco Configuration");

    
}


// Most commands are shared between curses and tty interfaces
// and handled here:
int action(int ch)
{
    if (!arco_ready) {
        printf("Ignored input. Arco has not started.\n");
        return false;  // Don't interact with Arco before it is ready.
    }
    switch (ch) {
      case 'A': // configuration dialog
        if (has_curses) {
            config_dialog();
        } else {
            printf("No audio dialog without curses. Edit .arco by hand.\n");
            for (int j = 0; j < arco_device_info.size(); j++) {
                puts(arco_device_info[j]);
            }
        }
        break;
      case 'B':  // "heartbeat" block counts on/off
      case 'b':
        o2_send_cmd("/arco/hb", 0, "i", ch == 'B');
        break;
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
      case 'p':  // print the audio tree
        o2_send_cmd("/arco/prtree", 0, "");
        break;
      case 'T':
        test_tone();
        break;
      case 't':
        o2_send_cmd("/arco/test1", 0, "s", "Test from arco (main)");
        break;
      default:
        break;
    }
    return false;
}


void dmaction()
{
    // update preferences
    for (int i = 0; i < dminfo.size(); i++) {
        if (dminfo[i].changed) {
            if (strstr(dminfo[i].label, "Input device:")) {
                prefs_set_in_name(arco_name_lookup(arco_in_id));
                p_in_id = arco_in_id;
            } else if (strstr(dminfo[i].label, "Output device:")) {
                prefs_set_out_name(arco_name_lookup(arco_out_id));
                p_out_id = arco_out_id;
            } else if (strstr(dminfo[i].label, "Input channels:")) {
                prefs_set_in_chans(arco_in_chans);
            } else if (strstr(dminfo[i].label, "Output channels:")) {
                prefs_set_out_chans(arco_out_chans);
            } else if (strstr(dminfo[i].label, "Buffer size:")) {
                prefs_set_buffer_size(arco_buffer_size);
            } else if (strstr(dminfo[i].label, "Latency:")) {
                prefs_set_latency(arco_latency_ms);
            } else if (strstr(dminfo[i].label, "Network enable:")) {
                prefs_set_network_enable(arco_network_enable);
            } else if (strstr(dminfo[i].label, "O2lite enable:")) {
                prefs_set_network_enable(arco_o2lite_enable);
            } else if (strstr(dminfo[i].label, "Internet enable:")) {
                prefs_set_internet_enable(arco_internet_enable);
            } else if (strstr(dminfo[i].label, "Mqtt enable:")) {
                prefs_set_mqtt_enable(arco_mqtt_enable);
            }
        }
    }
    if (host_network_enable != prefs_network_enable() ||
        host_o2lite_enable != prefs_o2lite_enable() ||
        host_internet_enable != prefs_internet_enable() ||
        host_mqtt_enable != prefs_mqtt_enable()) {
        printf("*** Save (P), Quit (Q), and restart for network\n"
               "***   preference changes to take effect.\n");
    }
}
