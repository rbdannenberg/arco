/* audioio.h -- audio dsp process for Arco
 *
 * Roger B. Dannenberg
 * Dec 2021
 */


// states for handshakes between main thread and audio callbacks and
// for sequencing audio open, close, reset, and application quit steps.
// Multiple states are handled when the audio callback is not running,
// so to identify these states, they all have the USE_MAIN_THREAD bit
// set.
#define USE_MAIN_THREAD (aud_state & USE_MAIN_THREAD_BIT)
const int USE_MAIN_THREAD_BIT = 256;

// fast check for "should we call audio callback?"
#define DO_AUDIO_CALLBACK (aud_state & DO_AUDIO_CALLBACK_BIT)
const int DO_AUDIO_CALLBACK_BIT = 512;

// fast check for "should we compute samples in callback?" (alternative
// is simply zero fill while starting/stopping)
#define COMPUTE_AUDIO (aud_state & COMPUTE_AUDIO_BIT)
const int COMPUTE_AUDIO_BIT = 1024;

const int IDLE = 0 + USE_MAIN_THREAD_BIT;
const int STARTING = 2 + USE_MAIN_THREAD_BIT;
const int STARTED = 3 + USE_MAIN_THREAD_BIT;
const int FIRST = 4;
const int RUNNING = 5 + DO_AUDIO_CALLBACK_BIT + COMPUTE_AUDIO_BIT;
const int STOPPING = 7 + DO_AUDIO_CALLBACK_BIT;
const int AUDIO_STOPPED = 8 + USE_MAIN_THREAD_BIT;
const int RESET_SENT = 9 + USE_MAIN_THREAD_BIT;
const int QUIT_SENT = 11 + USE_MAIN_THREAD_BIT;
const int FINISHED = 12 + USE_MAIN_THREAD_BIT;


// actual values used to open audio device
//extern char actual_in_name[80];
//extern char actual_out_name[80];

extern char ctrl_service_addr[64];  // where to send messages
        // this is set in audioio.cpp by /arco/open messages and is
        // "!" followed by the service followed by "/" so that you can
        // append the rest of the address in place.
extern int ctrl_service_addr_len;  // address len including "!" and "/"

int ctrl_set_service(const char *ctrlservice);
const char *ctrl_complete_addr(const char *method_name);

/* these are now local to audioio module:
extern int actual_in_id;
extern int actual_out_id;
extern int actual_in_chans;   // req_in_chans, but limited by device capacity
extern int actual_out_chans;  // req_out_chans, limited by device capacity
extern int actual_buffer_size;
extern int actual_latency;
*/

extern int aud_state;
//extern bool aud_is_reset;
//extern bool aud_quit_request;  // defer quit until audio is closed
//extern bool aud_reset_request;  // defer reset until audio is closed
//extern bool aud_start_request;  // defer start until reset completes

extern int64_t arco_frames_done;
extern int arco_blocks_done;

int audioio_initialize();
// void audioio_finish(Bridge_info *bridge);

// O2time arco_time(void *rock);

// void transpose(Sample *dst, Sample *src, int r, int c);

void arco_thread_poll();
/*SER void arco_thread_poll() PENT*/

//void host_reset_audio();
//void host_quit_audio();
