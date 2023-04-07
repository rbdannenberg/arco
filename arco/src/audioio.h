/* audioio.h -- audio dsp process for Arco
 *
 * Roger B. Dannenberg
 * Dec 2021
 */


const int UNINITIALIZED = 0;
const int IDLE = 1;
const int STARTING = 2;
const int STARTED = 3;
const int FIRST = 4;
const int RUNNING = 5;
const int STOPPING = 6;

// special Unit generator IDs:
const int ZERO_ID = 0;
const int ZEROB_ID = 1;
const int INPUT_ID = 2;
const int PREV_OUTPUT_ID = 3;
const int UGEN_BASE_ID = 4;


// actual values used to open audio device
extern char actual_in_name[80];
extern char actual_out_name[80];

/* these are now local to audioio module:
extern int actual_in_id;
extern int actual_out_id;
extern int actual_in_chans;   // req_in_chans, but limited by device capacity
extern int actual_out_chans;  // req_out_chans, limited by device capacity
extern int actual_buffer_size;
extern int actual_latency;
*/

extern int aud_state;
// communication with main thread: when aud_quit_request
// and aud_state == IDLE, call o2sm_finish()
extern int aud_quit_request;

extern int64_t arco_frames_done;
extern int64_t arco_blocks_done;

void audioio_initialize(Bridge_info *bridge);
void audioio_finish(Bridge_info *bridge);

O2time arco_time(void *rock);

void transpose(Sample *dst, Sample *src, int r, int c);

void arco_thread_poll();
/*SER void arco_thread_poll() PENT*/

void aud_forget(int id);


