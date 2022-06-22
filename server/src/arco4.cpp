/* arco4.cpp -- main program for arco4 server
 *
 * Roger B. Dannenberg
 * Dec 2021
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#ifdef __linux__
#include "ncurses.h"
#else
#include "curses.h"
#endif
#include "o2internal.h"  // need internal to offer bridge
#include "sharedmem.h"   // o2_shmem_inst_new()
#include "arcotypes.h"
#include "arco_ui.h"
#include "audioio.h"
#include "prefs.h"
#include "svprefs.h"
#include "arcoutil.h"
#include "ugen.h"
#include "zero.h"
#include "zerob.h"
#include "thru.h"

static int host_in_chans = 0;
static int host_out_chans = 0;
static int host_in_id = -1;
static int host_out_id = -1;
static int host_latency_ms = -1;
static int host_buffer_size = -1;

static int prefs_in_id = -1;
static int prefs_out_id = -1;

// leading 'c' means curses only, 'p' means plain terminal only

const char *help_strings[] = {
    " A - set Audio preferences",
    " B - block count messages ON",
    " b - block count messages OFF",
    " H - describe commands like this",
    "cP - Save latest selections to preferences",
    " p - Print audio ugen tree",
    " Q - Quit the program",
    " S - Start or Stop",
    " t - ask audio thread to print a test message",
    " T - test tone",
    NULL  // must terminate this list with NULL
};

const char *main_commands = "(A)udio device (S)tart/Stop (Q)uit (H)elp";

bool arco_ready = false;
bool has_curses = false;  // curses interface exists

static void arco4_initialize();

Bridge_info *arco_bridge = NULL;


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
        o2_send_cmd("/arco/open", 0, "iiiis", id, id, LATENCY_MS, BL, "host");
        o2_send_cmd("/arco/prtree", o2_time_get() + 1, "");
//        o2_send_cmd("/arco/open", 0, "iiii", 0, 1, LATENCY_MS, BL);
        /* Log some audio:
        o2_send_cmd("/arco/logaud", 0, "iis", 1, 1, "log.wav");
        o2_send_cmd("/arco/logaud", o2_time_get() + 1, "iis", 0, 1, "");
        */
    }
}


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
        o2_send_cmd("/arco/thru/new", 0, "iii", PREV_OUTPUT_ID, 2, ZERO_ID);
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


int main(int argc, char *argv[])
{
    prefs_read();
    has_curses = ui_init(200) >= 0;  // <0 means error, no curses interface
    o2_network_enable(false);
    o2_debug_flags("rs");
    o2_initialize("arco");
    o2_clock_set(NULL, NULL);  // we are the reference clock
    int rslt = o2_shmem_initialize();
    assert(rslt == O2_SUCCESS);
    arco_bridge = o2_shmem_inst_new();
    arco4_initialize();  // set up handler
    audioio_initialize(arco_bridge);

    int running = true;
    while (running) {
        running = !ui_poll(2);  // 2 ms polling period
        o2_poll();
        arco_thread_poll();
        if (!arco_ready) {
            arco_ready = (o2_status("arco") == O2_BRIDGE);
            if (!has_curses) {
                o2_send_cmd("/arco/hb", 0, "i", 0);  // turn messages off
            }
            o2_send_cmd("/arco/devinf", 0, "s", "/host/devinf");
        }
        // automation();
        o2_sleep(0.01);
    }
    ui_finish();
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

void host_open_audio()
{
    o2_send_cmd("/arco/open", 0, "iiiiiis", prefs_in_id, prefs_out_id,
                prefs_in_chans(), prefs_out_chans(),
                prefs_latency_ms(), prefs_buffer_size(), "host");
}


/* O2 INTERFACE: /host/reset ;  -- notice when audio is reset */
void host_reset(O2_HANDLER_ARGS)
{
    // Note: channels here are based on preferences. We try to open 
    // matching counts, but if devices cannot achieve the preferences,
    // inputs are expanded to the preferred count and outputs are mixed
    // down to the available output channels.
    printf("**** audio was reset, initializing %d ins %d outs ****\n",
           prefs_in_chans(), prefs_out_chans());
    o2_send_cmd("/arco/zero/new", 0, "i", ZERO_ID);
    o2_send_cmd("/arco/zerob/new", 0, "i", ZEROB_ID);
    o2_send_cmd("/arco/thru/new", 0, "iii", INPUT_ID, 1, ZERO_ID);
    o2_send_cmd("/arco/thru/new", 0, "iii", PREV_OUTPUT_ID, 2, INPUT_ID);

    // reset happens when we first start audio, so now we complete
    // starting audio by opening the audio stream:
    host_open_audio();
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


static void arco4_initialize()
{
    o2_service_new("host");
    // O2 INTERFACE INITIALIZATION: (machine generated)
    o2_method_new("/host/devinf", "s", host_devinfo, NULL, true, true);
    o2_method_new("/host/openaggr", "s", host_openaggr, NULL, true, true);
    o2_method_new("/host/starting", "iiiiii", host_starting, NULL, true, true);
    o2_method_new("/host/reset", "", host_reset, NULL, true, true);
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

// User preferences edited by the Audio dialog:
int arco_in_id = -1;
int arco_out_id = -1;
int arco_in_chans = -1;
int arco_out_chans = -1;
int arco_buffer_size = -1;
int arco_latency_ms = -1;


void audio_dialog()
{
    int in_id = -1;
    int out_id = -1;
    // look up device numbers in preferences
    for (int i = 0; i < arco_device_info.size(); i++) {
        const char *info = arco_device_info[i];
        int id = atoi(info);
        prefs_in_id = prefs_in_lookup(info, id);
        prefs_out_id = prefs_out_lookup(info, id);
    }
    ui_start_dialog();
    // "Current" values are displayed as information to user from
    // host_* variables unless there is no actual value yet,
    // in which case we assume the preferences will give us a value:
    ui_int_field("Input device:   ", &arco_in_id, -1, 100,
                 host_in_id, prefs_in_id);
    ui_int_field("Output device:  ", &arco_out_id, -1, 100,
                 host_out_id, prefs_out_id);
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
      case 'S':  // start/stop
        if (arco_started) {
            printf("Closing audio devices.\n");
            o2_send_cmd("/arco/close", 0, "");
        } else if (arco_reset) {
            /* printf("Open device %d (in, %d chans) and %d (out, %d chans), "
                   "latency %d ms, buffer size %d\n", prefs_in_id,
                   prefs_in_chans(), prefs_out_id, prefs_out_chans(),
                   prefs_latency_ms(), prefs_buffer_size()); */
            host_open_audio();
        } else {  // first time we are starting
            o2_send_cmd("/arco/reset", 0, "s", "host");
            arco_reset = true;
        }
        arco_started = !arco_started;
        break;
      case 'P':  // save parameters
        prefs_write();
        break;
      case 'p':  // print the audio tree
        o2_send_cmd("/arco/prtree", 0, "");
        break;
      case 'Q':  // quit
        return true;
      case 'T':
        test_tone();
        break;
      case 't':
        o2_send_cmd("/arco/test1", 0, "s", "Test from arco4 (main)");
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
                prefs_in_id = arco_in_id;
            } else if (strstr(dminfo[i].label, "Output device:")) {
                prefs_set_out_name(arco_name_lookup(arco_out_id));
                prefs_out_id = arco_out_id;
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
