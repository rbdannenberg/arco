/* arco.cpp -- main program for arco server
 *
 * Roger B. Dannenberg
 * Dec 2021
 */

#include "o2internal.h"  // need internal to offer bridge
#include <fcntl.h>
#include <unistd.h>
#ifdef __linux__
#include "ncurses.h"
#else
#include "curses.h"
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

// User preferences edited by the Audio dialog:
int arco_in_id;
int arco_out_id;
int arco_in_chans;
int arco_out_chans;
int arco_buffer_size;
int arco_latency_ms;

static int host_in_chans = 0;
static int host_out_chans = 0;
static int host_in_id = -1;
static int host_out_id = -1;
static int host_latency_ms = -1;
static int host_buffer_size = -1;


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

static bool open_requested = false;
static bool close_requested = false;
static bool reset_requested = false;
static bool quit_requested = false;
static int server_aud_state = IDLE;
static bool server_aud_is_reset = false;
#define aud_state THIS_IS_PRIVATE_TO_THE_AUDIO_SERVICE

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

int main(int argc, char *argv[])
{
    prefs_read();

    printf("main: initial latency %d\n", prefs_latency_ms());
    has_curses = ui_init(200) >= 0;  // <0 means error, no curses interface
    o2_network_enable(false);
    o2_debug_flags("");
    o2_initialize("arco");
    o2_clock_set(NULL, NULL);  // we are the reference clock
    host_initialize();  // set up handlers
    int err;
    if ((err = arco_initialize())) {
        printf("FATAL ERROR %d in arco_initiailize. Exiting now.\n", err);
        exit(1);
    }

    int running = true;
    int shutting_down = false;
    while (server_aud_state != FINISHED || shutting_down) {
        ui_poll(2);  // 2 ms polling period
        if (server_aud_state == FINISHED) {  // User entered the Quit command
            if (!shutting_down) {
                shutting_down = true;  // wait for O2sm_info to be deleted
            } else {  // test for O2sm_info deleted before exiting loop
                shutting_down = (o2_shmem_inst_count() > 0);
                // when no more shared memory instances, shutting_down becomes
                // false, and this will be the last time through the loop
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
    if (server_aud_state == STOPPING || server_aud_state == IDLE) {
        return 1;  // already stopping or stopped
    }
    if (server_aud_state == STARTING) {
        close_requested = true;
        return 0;
    }
    assert(server_aud_state == RUNNING);
    server_aud_state = STOPPING;
    o2_send_cmd("/arco/ctrl", 0, "s", "host");
    o2_send_cmd("/arco/close", 0, "");
}


int host_open_audio()
{
    if (server_aud_state == STARTING || server_aud_state == RUNNING) {
        return 1;  // already starting or running
    }
    if (server_aud_state == STOPPING) {
        open_requested = true;
        return 0;
    }
    assert(server_aud_state == IDLE);
    server_aud_state = STARTING;
    o2_send_cmd("/arco/open", 0, "iiiiii", p_in_id, p_out_id,
                prefs_in_chans(), prefs_out_chans(),
                prefs_latency_ms(), prefs_buffer_size());
}


void host_quit_audio()
{
    if (server_aud_state == STARTING || server_aud_state == RUNNING) {
        quit_requested = true;
        host_close_audio();
        return;
    }
    if (server_aud_state == STOPPING) {
        quit_requested = true;
        return;
    }
    assert(server_aud_state == IDLE);
    o2_send_cmd("/arco/quit", 0, "");
}


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


/* O2 INTERFACE: /host/reset int32 status;
*/
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
        arco_warn("Server expected state to be STARTING but got %d\n",
                  server_aud_state);
        return;
    }
    server_aud_state = RUNNING;
    if (close_requested) {
        close_requested = false;
        host_close_audio();
    }
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
    if (quit_requested) {
        host_quit_audio();
        return;
    }
    if (reset_requested) {
        reset_requested = false;
        host_reset_audio();
    }
    if (open_requested) {
        open_requested = false;
        host_open_audio();
    }
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
    o2_method_new("/host/reset", "i", host_reset, NULL, true, true);
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

void audio_dialog()
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
    arco_buffer_size = p_buffer_size;
    arco_latency_ms = p_latency_ms;

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
    ui_run_dialog("Audio Input/Output Configuration");

    
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
      case 'A': // audio device
        if (has_curses) {
            audio_dialog();
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
      case 'R':  // reset: remove all unit generators
        host_reset_audio();
        break;
      case 'S':  // start/stop
        if (server_aud_state == RUNNING) {
            printf("Closing audio devices.\n");
            host_close_audio();
        } else if (server_aud_state == IDLE) {
            if (server_aud_is_reset) {
                /* printf("Open device %d (in, %d chans) and %d "
                   "(out, %d chans), "
                   "latency %d ms, buffer size %d\n", p_in_id,
                   prefs_in_chans(), p_out_id, prefs_out_chans(),
                   prefs_latency_ms(), prefs_buffer_size()); */
                host_open_audio();
            } else {  // first time we are starting
                open_requested = true;
                host_reset_audio();
            }
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
            }
        }
    }
}
