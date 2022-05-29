/* arco4.cpp -- main program for arco4 server
 *
 * Roger B. Dannenberg
 * Dec 2021
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include "curses.h"
#include "o2internal.h"  // need internal to offer bridge
#include "sharedmem.h"   // o2_shmem_inst_new()
#include "arcotypes.h"
#include "arco_ui.h"
#include "audioio.h"
#include "prefs.h"

const char *help_strings[] = {
    "A <in id> <out id> - select Audio device to open",
    "B <n> - select Block size for Arco unit generators",
    "C <in> <out> - set desired input and output channels",
    "H - describe commands like this",
    "I - get Audio IDs and Info",
    "L <n> - select Latency in msn",
    "P - Save latest selections to preferences",
    "Q - Quit the program",
    "S - Start or Stop",
    NULL };

const char *main_commands = "(A)udio device (B)lock size (L)atency "
                            "(S)tart/Stop (Q)uit (H)elp";

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
        o2_send_cmd("/arco/open", 0, "iiiis", id, id, LATENCY_MS, BL, "actl");
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
    ui_init(200);
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
        // automation();
        o2_sleep(0.01);
    }
    ui_finish();
    o2_finish();
}


/* O2 INTERFACE: /host/arcoinfo string info;
   Receive and print an /arco/devinf reply, which looks like:
        "<id> - <api_name> : <name> (<in> ins, <out> outs)"
*/
static void host_audinfo(O2_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    char *info = argv[0]->s;
    // end unpack message

    printf("%s\n", info);
}


static void arco4_initialize()
{
    o2_service_new("host");
    // O2 INTERFACE INITIALIZATION: (machine generated)
    o2_method_new("/host/arcoinfo", "s", host_audinfo, NULL, true, true);
    o2_method_new("/host/openaggr", "s", host_openaggr, NULL, true, true);
    // END INTERFACE INITIALIZATION
}


bool arco_started = false;

static void got_device_id(const char *s)
{
    int n = sscanf(s, "%d %d", &arco_in_device, &arco_out_device);
    if (n == 1) {
        arco_out_device = arco_in_device;
    }
}


static void got_latency(const char *s)
{
    arco_latency = atoi(s);
}


static void got_channels(const char *s)
{
    int in_chans, out_chans;
    int n = sscanf(s, "%d %d", &arco_in_chans, &arco_out_chans);
    if (n == 1) {
        arco_out_chans = arco_in_chans;
    }
}


static void audio_dialog()
{
    ui_start_dialog();
    ui_int_field("Input device:   ", &arco_in_device, -1, 100, actual_in_device);
    ui_int_field("Output device:  ", &arco_out_device, -1, 100, actual_out_device);
    ui_int_field("Input channels: ", &arco_in_chans, -1, 100, actual_in_chans);
    ui_int_field("Output channels:", &arco_out_chans, -1, 100, actual_out_chans);
    ui_int_field("Buffer size     ", &arco_buffer_size, -1, 1024, actual_buffer_size);
    ui_int_field("Latency:        ", &arco_latency, -1, 1024, actual_latency);
    ui_run_dialog("Audio Input/Output Configuration");
}



// Most commands are shared between curses and tty interfaces
// and handled here:
int action(int ch)
{
    switch (ch) {
      case 'A': // audio device
        // ui_get_string("Audio Device ID (in [out])", got_device_id);
        audio_dialog();
        break;
      case 'B':  // block size
        break;
      case 'C':
        ui_get_string("Channels (in out), e.g. \"1 2\"", got_channels);
        break;
      case 'I':
        o2_send_cmd("/arco/devinf", 0, "s", "/host/arcoinfo");
        break;
      case 'L':  // latency
        ui_get_string("Latency (ms)", got_latency);
        break;
      case 'S':  // start/stop
        if (arco_started) {
            printf("Closing audio devices.\n");
            o2_send_cmd("/arco/close", 0, "");
        } else {
            printf("Open device %d (in) and %d (out), latency %d ms, "
                   "buffer size %d\n", arco_in_device, arco_out_device,
                   arco_latency, arco_buffer_size);
            o2_send_cmd("/arco/open", 0, "iiiis", arco_in_device,
                    arco_out_device, arco_latency, arco_buffer_size, "actl");
        }
        arco_started = !arco_started;
        break;
      case 'P':  // save parameters
        prefs_set_in_device(actual_in_name);
        prefs_set_out_device(actual_out_name);
        prefs_set_in_chans(req_in_chans);
        prefs_set_out_chans(req_out_chans);
        prefs_set_latency(actual_latency);
        prefs_set_buffer_size(actual_buffer_size);
        prefs_write();
        break;
      case 'Q':  // quit
        return true;
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

}
