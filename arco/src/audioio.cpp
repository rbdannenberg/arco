/* audioio.cpp -- audio dsp process for Arco
 *
 * Roger B. Dannenberg
 * Dec 2021
 */

/*
Supports running O2SM even when audio callbacks are not functioning.

Let's assume that it can take awhile for audio to start up, so we'll
get it running first, then switch over to actually using it.
 - states are UNINITIALIZED, IDLE, STARTING, STARTED, FIRST, RUNNING,
   STOPPING:
   UNINITIALIZED->IDLE by calling Pa_Initialize()
   IDLE->STARTING done by thread when starting PortAudio
   STARTING->STARTED done by callback when it happens - signals thread
   STARTED->FIRST done by thread - acknowledges the STARTED signal
   FIRST->RUNNING done by callback to effect handoff and time adjust
   RUNNING->STOPPING done by callback upon request to shut down
   STOPPING->IDLE done by thread to close log file, etc.
   These are the atomic actions that mark handoffs.
   The callback does nothing except output zero (silence) unless 
   the state is FIRST which immediately becomes RUNNING.
   STOPPING is set at the very end of the callback, so once it is
   set, it is immediately safe to run the audio zone on another thread.

 - When audio processing starts, we set source to audio thread and
   start incrementing the time when each block arrives.
 - When audio processing stops, we set precise_offset and then set
   source to either ui or midi thread. 
     We want: precise_secs() + precise_offset == GETTIME, so
     set precise_offset = GETTIME - precise_secs()

Audio Processing

1. Audio input has actual_in_chans channels. These are
de-interleaved and copied to the *output* of:

2. aud_input_ug. If there are too many input channels, extras are
ignored. (But initially, we only open as many channels as we can 
use.) If there are too few, extra channels are zero.

3. Audio graphs can reference aud_input_ug, which acts as a source, to
get samples.

4. run_set contains unit generators that must be run, but which are
not producing audio output, e.g., an audio feature computation. These
are computed after input is available.

5. synthesize() is run and computed audio is written to aud_output_ug,
which has a vector of channels.

6. output is copied to the outputs of aud_prev_output_ug. (These
outputs can be accessed in the next block, allowing feedback with a
1-block delay.)

7. output is copied in interleaved form to a buffer

8. If aud_out_chans > actual_out_chans, the extra output channels
are "wrapped" and summed into the first actual_out_chans. E.g. if
there are 6 output channels but only 2 actual channels, channels 2 and
4 are added to 0, and channels 3 and 5 are added to 1, so everything
is routed to channels 0 or 1.

9. The buffer is copied to the output samples area passed in by the
callback. If aud_out_chans <= actual_out_chans, there is no
wrapping, and the output samples memory block is used as the buffer to
avoid an extra copy. If necessary extra actual output channels are
filled with zero.

*/

#include <unistd.h>
#include <stdio.h>
#include <algorithm>  // min
#include <xmmintrin.h>
#include <string.h>
#include "sndfile.h"
#include "portaudio.h"
#include "o2internal.h"
#include "sharedmemclient.h"
#include "prefs.h"
#include "arcotypes.h"
#include "arcoutil.h"
#include "ugen.h"
#include "recperm.h"
#include "audioio.h"
#include "thru.h"


using std::min;

// Note: audio processing can be carried out by more than one thread.
// When the audio callback happens, we set the thread_local o2_ctx
// variable to &aud_o2_ctx. When audio is not running, someone should
// call arco_thread_poll(), which saves o2_ctx on the stack, sets
// o2_ctx to &aud_o2_ctx, checks for messages and events, then restores
// o2_ctx.
O2_context aud_o2_ctx;  // O2 context for audio thread

int aud_state = UNINITIALIZED;
bool aud_reset_request = false;
int64 aud_frames_done = 0;
int64 aud_blocks_done = 0;

int arco_in_device = -1;
int arco_out_device = -1;
int arco_in_chans = -1;
int arco_out_chans = -1;
int arco_buffer_size = -1;
int arco_latency = -1;

int req_in_chans = -1;   // preferred or specified count
int req_out_chans = -1;  // preferred or specified count
int actual_in_chans;     // number of channels in callback
int actual_out_chans;    // number of channels in callback
int actual_latency = -1;
int actual_buffer_size = -1;
int actual_in_device = -1;
int actual_out_device = -1;
char actual_in_name[80] = "";
char actual_out_name[80] = "";

static int close_request;
static PaStream *audio_stream;

static Vec<int> run_set;  // UG ID's to refresh every block
static Vec<int> output_set;  // list of UG ID's to sum to form output

static int cpuload = 0;
static char audioinlogfilename[ARCO_STRINGMAX];
static char audiooutlogfilename[ARCO_STRINGMAX];
static SNDFILE *audioinlogfile = NULL;
static SNDFILE *audiooutlogfile = NULL;
static int log_input = false;
static int log_output = false;


static int underflow_count = 0;

/************ CLOCK *************/

/* The O2 callback always just returns arco_wall_time.  When audio is
 * running, we increment arco_wall_time by the block period each
 * callback. When audio is not running, we are still polling
 * arco_poll(), which uses o2_native_time() to set
 * arco_wall_time. Since arco_poll() is asynchronous (but hopefully
 * often), we keep a constant offset to o2_native_time(), re-setting
 * the offset when audio stops running and we switch over to the
 * o2_native_time() source.
 */

static double arco_wall_time = 0;
static double arco_wall_time_offset = 0;


// get the run time since starting. When audio is running, time
// advances with each audio callback.
O2time arco_time(void *rock)
{
    return arco_wall_time;
}


static void switch_to_audio_time()
{
    return;  // nothing to do
}


static void switch_to_o2_time()
{
    // arco_wall_time should be: o2_native_time() + arco_wall_time_offset,
    // so:
    arco_wall_time_offset = arco_wall_time - o2_native_time();
}


// interleave or de-interleave audio: 
//     src is a sequence of r rows, each of size c
//     dst will be a sequence of c rows, each of size r
// e.g., to interleave, c is BL and r is channels;
// to de-interleave, c is channels and r is BL 
void transpose(Sample_ptr dst, Sample_ptr src, int r, int c)
{
    for (int ci = 0; ci < c; ci++) {
        Sample_ptr srcptr = src + ci;
        for (int ri = 0; ri < r; ri++) {
            *dst++ = *srcptr;
            srcptr += c;
        }
    }
}


static bool forget_ugen_id(int id, Vec<int> &set)
{
    for (int i = 0; i < set.size(); i++) {
        if (set[i] == id) {
            set.remove(i);
            return true;
        }
    }
    return false;
}


/* O2SM INTERFACE: /arco/test1 string s;
   To test if messages are being received, this function
   will print the string on receipt (followed by newline)
*/
static void arco_test1(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    const char *s = argv[0]->s;
    // end unpack message

    printf("%s\n", s);
}


/* O2SM INTERFACE: /arco/prtree ;
   Print a tree of all unit generators
*/
static void arco_prtree(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    // end unpack message

    arco_print("------------ Ugen Tree ----------------\n");
    for (int i = 0; i < output_set.size(); i++) {
        Ugen_ptr ugen = ugen_table[output_set[i]];
        if (ugen->flags & UGEN_MARK) { // special case: print again
            ugen->flags &= ~UGEN_MARK;
        }
        ugen->print_tree(0, true, "in audioio output_set");
    }
    // clear the marks by traversing tree again
    for (int i = 0; i < output_set.size(); i++) {
        Ugen_ptr ugen = ugen_table[output_set[i]];
        ugen->print_tree(0, false, "");
    }
    arco_print("---------------------------------------\n");
}


/* O2SM INTERFACE: /arco/prtree ;
   Print a tree of all unit generators
*/
static void arco_reset(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    const char *s = argv[0]->s;
    // end unpack message
    set_control_service(s);
    aud_reset_request = true;
}


/* O2SM INTERFACE: /arco/devinf string address;
   Send device info to a given address. Info messages (one per 
   device) have one string parameter, and last device is followed
   by an empty string. Reply string format is
        "<id> - <api_name> : <name> (<in> ins, <out> outs)"
*/
static void arco_devinf(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    char *address = argv[0]->s;
    // end unpack message

    char audio_info_buff[ARCO_STRINGMAX];
    if (aud_state == UNINITIALIZED) {
        PaError err = Pa_Initialize();
        if (err != paNoError) return;
        aud_state = IDLE;
    }
    PaDeviceIndex num_devs = Pa_GetDeviceCount();
    for (int id = 0; id < num_devs; id++) {
        const PaDeviceInfo *info = Pa_GetDeviceInfo(id);
        const char *name = info->name;
        const PaHostApiInfo *api_info = Pa_GetHostApiInfo(info->hostApi);
        const char *api_name = api_info->name;
        int in_chans = info->maxInputChannels;
        int out_chans = info->maxOutputChannels;
        snprintf(audio_info_buff, ARCO_STRINGMAX,
             "%d - %s : %s (%d ins, %d outs)",
             id, api_name, name, in_chans, out_chans);
        audio_info_buff[ARCO_STRINGMAX - 1] = 0; // just in case
        o2sm_send_cmd(address, 0, "s", audio_info_buff);
    }
    // terminate list with empty string:
    o2sm_send_cmd(address, 0, "s", "");
}


static void busywork(int i) { return; }


// copy from interleaved src with src_chans to deinterleaved dest with
// dest_chans. Zero pad if needed, and drop src channels if there are too many.
//
void copy_deinterleave(Sample_ptr dest, int dest_chans,
                       Sample_ptr src, int src_chans)
{
    int chans = min(src_chans, dest_chans);
    for (int c = 0; c < chans; c++) {
        // compute one channel of input by de-interleaving src
        Sample_ptr s = src + c;
        for (int i = 0; i < BL; i++) {
            *dest++ = *s;
            s += src_chans;  // step to next frame
        }
    }
    if (dest_chans > src_chans) {
        // we need to zero any remaining channels
        memset(dest, 0, (dest_chans - src_chans) * BLOCK_BYTES);
    }
}


// float vector x += y
void sum_from_into(Sample_ptr x, Sample_ptr y, int n)
{
    for (int i = 0; i < n; i++) {
        *x++ += *y++;
    }
}

// mix from buffer to output. Output channels are sum of input
// channels mod actual_out_chans, e.g. 0+2->0, 1+3->1
//
void mix_down_interleaved(Sample_ptr output, int actual_out_chans,
                          Sample_ptr buffer, int aud_out_chans)
{
    // Note: aud_out_chans > actual_out_chans, and
    //       buffer is bigger than output
    // just copy the first actual_out_chans of each frame
    Sample_ptr src = buffer;
    Sample_ptr dst = output;
    for (int i = 0; i < BL; i++) {
        // copy actual_out_chans frames
        for (int j = 0; j < actual_out_chans; j++) {
            *dst++ = src[j];
        }
        src += aud_out_chans;  // skip to next frame
    }
    // add in the rest, actual_out_chans at a time
    for (int c = actual_out_chans; c < aud_out_chans;
         c += actual_out_chans) {
        // mix up to next actual_out_chans into first actual_out_chans
        // but if there are not actual_out_chans left, mix how many
        // are left:
        int howmany = min(actual_out_chans, aud_out_chans - c);
        Sample_ptr src = buffer + c;
        Sample_ptr dst = output;
            for (int i = 0; i < BL; i++) {
                // copy howmany frames
                for (int j = 0; j < howmany; j++) {
                    *dst++ += src[j];
                }   
                src += aud_out_chans;
            }
    }
}    
    
    
// add channels to buffer while copying to output. Output is to be
// interleaved, but source is not
//
void copy_interleaved_with_zero_fill(Sample_ptr output, int actual_out_chans,
                                     Sample_ptr source, int aud_out_chans)
{
    for (int i = 0; i < BL; i++) {  // copy BL frames
        int c;
        Sample_ptr src = source + i;  // offset by frame, stride is BL
        for (c = 0; c < aud_out_chans; c++) {  // copy existing channels
            *output++ = *src;
            src += BL;
        }
        while (c++ < actual_out_chans) {  // zero fill 
            *output++ = 0.0F;
        }
    }
}


// called for each block of BL * actual_in_chans samples
// input is interleaved, output is computed with interleaving
// But input is de-interleaved and copied to the first actual_in_chans
// of aud_input_ug. For output, we compute BL * aud_out_chans. If we
// have enough actual channels, then we interleave to output and maybe
// write output to the log file. If we don't have enough actual samples,
// we allocate a buffer and interleave there in a separate interleave step.
// Then to form output, we have to mix channels.
//
static int callback_entry(float *input, float *output,
                          PaStreamCallbackFlags flags)
{
    o2_ctx = &aud_o2_ctx;  // make thread context available to o2 functions
    
    if (aud_state == RUNNING) {
        // arco time will suffer from audio computation time jitter, and
        // if PortAudio blocks are large (e.g. multiple Arco blocks), time
        // will tend to advance in big steps on each callback.
        arco_wall_time += BP; // time is advanced by block period
    } else {
        memset(output, 0, BL * sizeof(float) * actual_out_chans);
        return paContinue;
    }
    // we are RUNNING on the callback

    _mm_setcsr(_mm_getcsr() | 0x8040); // SSE Denormals handling: At least
    // some Intel processors are *extremely* slow handling denormals
    // which can occur with exponential decay of audio to
    // silence. Rounding to zero is perfectly acceptable for audio and
    // avoids spikes in computation that cause processing to be slower
    // than real time. This enables denormals-are-zero (DAZ) and
    // flush-to-zero (FTZ) flags for SSE.

    // Impose artificial load, but stop for a second if underflows happen.
    // I don't think the optimizer will remove a function call.
    if (cpuload && underflow_count > 1000) {
        for (long i = 0; i < cpuload; i++) busywork(i);
    }

    if (log_input) {
        sf_writef_float(audioinlogfile, input, BL);
    }
    
    /*
    if (blocks_done % 1000 == 0 && actual_in_chans > 0) {
        arco_print("Audio callback %ld input %g\n", blocks_done, *input);
    }
    */

    o2sm_poll();
    aud_frames_done += BL;
    Ugen_ptr input_ug = ugen_table[INPUT_ID];
    Sample_ptr aout;
    aud_blocks_done++;  // bring everyone up to this count
    if (input_ug && (aout = &input_ug->output[0])) {
        copy_deinterleave(aout, input_ug->chans, input, actual_in_chans);
        input_ug->set_current_block(aud_blocks_done);
    }

    for (int i = 0; i < run_set.size(); i++) {
        // since our current_block was advanced after input was read
        // it is the number we want
        Ugen_ptr ug = ugen_table[run_set[i]];
        if (ug) {
            ug->run(aud_blocks_done);
        }
    }

#ifdef TESTTONE
    // this generates a triangle wave to bypass Arco synthesis and just
    // output audio -- used for debugging and latency feasibility test.
    for (int i = 0; i < BL; i++) {
        float phase = (i - 16) / 16.0;  // -1 to +1
        output[i * 2] = (abs(phase) - 0.5) * 0.1;
        output[i * 2 + 1] = output[i * 2];
    }
#else
    Ugen_ptr prev_output = ugen_table[PREV_OUTPUT_ID];
    if (actual_out_chans > 0 && output_set.size() > 0 &&
        prev_output && prev_output->chans > 0) {

        // Compute the output to be copied to prev_output, then copy,
        // then interleave and mix down to actual output.
        // Use the actual output as a buffer for the first step if it
        // is big enough; otherwise, allocate a buffer on the stack:
        Sample_ptr buffer = NULL;
        int aud_out_chans = prev_output->chans;
        if (actual_out_chans >= aud_out_chans) {
            buffer = output;  // output is big enough
        } else {
            buffer = (Sample_ptr) alloca(aud_out_chans * sizeof(Sample));
        }

        Sample_ptr outptr;

        // compute all outputs, first is copy and zero fill, rest sum to output
        for (int i = 0; i < output_set.size(); i++) {
            Ugen_ptr output_ug = ugen_table[output_set[i]];
            output_ug->run(aud_blocks_done);
            outptr = &output_ug->output[0];
            int n = min(output_ug->chans, aud_out_chans);
            if (i == 0) {  // copy and fill
                memcpy(buffer, outptr, n * BLOCK_BYTES);
                if (n < aud_out_chans) {
                    memset(buffer + BL * n, 0,
                           (aud_out_chans - n) * BLOCK_BYTES);
                }
            } else {  // 2nd sound: add to buffer, no fill needed
                sum_from_into(buffer, outptr, n * BL);
            }
            // do we need to wrap extra channels in outptr?
            while (output_ug->chans > n) {
                int count = min(output_ug->chans - n, aud_out_chans);
                sum_from_into(buffer, outptr + BL * n, count * BL);
                n += count;
            }
        }

        // copy computed output to prev_output
        outptr = &prev_output->output[0];
        memcpy(outptr, buffer, aud_out_chans * BLOCK_BYTES);

        // interleave the computed output back to buffer
        transpose(buffer, outptr, aud_out_chans, BL);

        // now we have interleaved graph output, log it if enabled:
        if (log_output) {
            sf_writef_float(audiooutlogfile, buffer, BL);
        }

        // now we need actual_out_chans of interleaved samples in output
        // case 1: buffer != output means we have to mix down because
        //    audio graph output chans > actual output chans.
        // case 2: the channel counts do not match, and actual output is
        //    bigger than graph output, so interleave with zero fill
        // case 3: we've already computed output because the graph
        //    channel count matches actual output chans

        // do we need to mix down channels for output?
        if (buffer != output) { // yes - case 1
            mix_down_interleaved(output, actual_out_chans,
                                 buffer, aud_out_chans);
        } else if (actual_out_chans != aud_out_chans) {  // case 2
            assert(actual_out_chans > aud_out_chans);
            copy_interleaved_with_zero_fill(output, actual_out_chans,
                                            outptr, aud_out_chans);
        }  // else output *is* buffer and samples are ready to go
            
        // audio_out has a one-block (32-sample == BL) delay; otherwise
        // (for example) Ivu runs to compute clipping before we copy
        // output to audio_out, so audio_out will need to be refreshed,
        // but that would invoke Audioio::real_run() which is not allowed.
        // (We could have real_run() run synthesize(), but what if
        // synthesize tries to access Ivu's output? It's recursive.
        prev_output->set_current_block(aud_blocks_done + 1);
    } else if (actual_out_chans > 0) {  // no synthesized output, so zero output
        memset(output, 0, BL * actual_out_chans * sizeof(*output));
    }
#endif
    if (close_request) {
        return paComplete;
    }
    /*
    for (int i = 0; i < BL * 2; i += 2) {
        printf("%d:[%g, %g] ", i / 2, output[i], output[i + 1]);
    }
    printf("\n");
     */
    return paContinue;
}


static int pa_callback(const void *input, void *output,
                unsigned long frame_count,
                const PaStreamCallbackTimeInfo* time_info,
                PaStreamCallbackFlags status_flags,
                void *userData)
{
    int result = paContinue;
    float *in = (float *) input;
    float *out = (float *) output;
    if (aud_blocks_done % 1000 == 0) printf("%lld blocks\n", aud_blocks_done);
    if (status_flags) {
        // only report this once per 100 callbacks
        if (underflow_count > 100) {
            underflow_count = 0; // no report until 100 callbacks elapse
            arco_print("Audio status %ld%s, frame count %ld\n",
                       status_flags, (status_flags & paOutputUnderflow ?
                                      " (OutputUnderflow)" : ""), frame_count);
        }
    }
    underflow_count++;
    
    if (aud_state == RUNNING) {
        // input and output are interleaved, so each arco block has length
        // BL * chans, and each block is at in + i * actual_in_chans.
        // in case PortAudio frame_count > BL, synthesize multiple blocks:
        for (unsigned long i = 0; i < frame_count; i += BL) {
            int r = callback_entry(in + i * actual_in_chans,
                    out + i * actual_out_chans, status_flags);
            if (r != paContinue) result = r;
        }
        if (result != paContinue || aud_reset_request) {
            // since some time has elapsed computing audio (maybe), call
            // precise_secs() again to compute precise offset.
            switch_to_o2_time();
            aud_state = STOPPING;
            result = paComplete;
        }
    } else if (aud_state == FIRST) {
        switch_to_audio_time();
        aud_state = RUNNING;
    } else if (aud_state == STARTING) {
        aud_state = STARTED;
    }
    return result;
}


/* O2SM INTERFACE:  /arco/output int32 id; -- add Ugen to output set */
static void arco_output(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32 id = argv[0]->i;
    // end unpack message
    ANY_UGEN_FROM_ID(ugen, id, "aud_output");

    if (ugen->rate == 'a') {
        // make sure this is not already in output set
        for (int i = 0; i < output_set.size(); i++) {
            if (output_set[i] == id) {
                arco_warn("/arco/output %d already in output set\n", id);
                return;
            }
        }
        output_set.push_back(id);
    } else {
        arco_warn("arco_output %d not audio rate, ignored\n", id);
    }
}


/* O2SM INTERFACE:  /arco/mute int32 id; -- remove output Ugen */
static void arco_mute(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32 id = argv[0]->i;
    // end unpack message

    if (!forget_ugen_id(id, output_set)) {
        arco_warn("/arco/mute %d not in output set, ignored\n", id);
    }
}


/* O2SM INTERFACE:  /arco/open
       int32 in_device, int32 out_device,
       int32 latency_ms, int32 buffer_size,
       string ctrlservice;
   Open audio device(s) for input and output and begin processing
   audio.
*/
static void arco_open(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32 in_device = argv[0]->i;
    int32 out_device = argv[1]->i;
    int32 latency_ms = argv[2]->i;
    int32 buffer_size = argv[3]->i;
    char *ctrlservice = argv[4]->s;
    // end unpack message

    PaError err;
    PaDeviceIndex num_devices = 0;
    PaTime suggested_latency;
    PaStreamParameters input_params;
    PaStreamParameters *input_params_ptr = &input_params;
    PaStreamParameters output_params;
    PaStreamParameters *output_params_ptr = &output_params;
    bool got_input = false;
    bool got_output = false;

    if (aud_state != UNINITIALIZED && aud_state != IDLE) {
        arco_warn("aud_open called but audio is already opened");
        return;
    }

    if (set_control_service(ctrlservice)) {
        return;  // error -- probably string too long for service name
    };

    if (aud_state == UNINITIALIZED) {
        err = Pa_Initialize();
        if (err != paNoError) goto done;
        aud_state = IDLE;
    }

    aud_frames_done = 0;
    num_devices = Pa_GetDeviceCount();
    // default is to follow the input size in the audio graph:
    req_in_chans = (ugen_table[INPUT_ID] ? ugen_table[INPUT_ID]->chans : 0);
    // preference file can override the graph:
    req_in_chans = prefs_in_chans(req_in_chans);
    // gui can override the preference file:
    req_in_chans = (arco_in_chans >= 0 ? arco_in_chans : req_in_chans);

    req_out_chans = (ugen_table[PREV_OUTPUT_ID] ?
                         ugen_table[PREV_OUTPUT_ID]->chans : 0);
    req_out_chans = prefs_out_chans(req_out_chans);
    // gui can override the preference file:
    req_out_chans = (arco_out_chans >= 0 ?
                        arco_out_chans : req_out_chans);

    actual_in_device = Pa_GetDefaultInputDevice();
    actual_out_device = Pa_GetDefaultOutputDevice();

    // override Pa defaults with preferences:
    const PaDeviceInfo *info;
    for (PaDeviceIndex i = 0; i < num_devices; i++) {
        info = Pa_GetDeviceInfo(i);
        arco_print("Device %d: %s\n", i, info->name);
        actual_in_device = prefs_in_device(info->name, i, &actual_in_device);
        actual_out_device = prefs_in_device(info->name, i, &actual_out_device);
    }
    
    // /arco/open device specs override preferences
    if (in_device >= 0) actual_in_device = in_device;
    if (out_device >= 0) actual_out_device = out_device;

    // GUI overrides /arco/open
    actual_in_device = (arco_in_device >= 0 ? arco_in_device :
                                              actual_in_device);
    actual_out_device = (arco_out_device >= 0 ? arco_out_device :
                                                actual_out_device);

    suggested_latency = 0;
    if (req_in_chans == 0) {
        actual_in_device = paNoDevice;
        arco_print("Zero input channels, so no input device to open.\n");
        actual_in_name[0] = 0;  // empty string
    } else {
        info = Pa_GetDeviceInfo(actual_in_device);
        strncpy(actual_in_name, info->name, 80);
        actual_in_name[79] = 0;  // make sure it is terminated
        arco_print("Aura selects input device: %s\n", info->name);
        actual_in_chans = req_in_chans;
        if (info->maxInputChannels < req_in_chans) {
            arco_print("\n    WARNING: only %d input channels available.\n",
                       info->maxInputChannels);
            actual_in_chans = info->maxInputChannels;
        }
        suggested_latency = info->defaultLowInputLatency;
    }

    if (req_out_chans == 0) {
        actual_out_device = paNoDevice;
        arco_print("Zero output channels, so no output device to open.\n");
        actual_out_name[0] = 0;  // empty string
    } else {
        info = Pa_GetDeviceInfo(actual_out_device);
        strncpy(actual_out_name, info->name, 80);
        actual_out_name[79] = 0;  // make sure it is terminated
        arco_print("Aura selects output device: %s\n", info->name);
        actual_out_chans = req_out_chans;
        if (info->maxOutputChannels < req_out_chans) {
            arco_print("\n    WARNING: only %d output channels available.\n",
                       info->maxOutputChannels);
            actual_out_chans = info->maxOutputChannels;
        }
        // get max of suggested input and output latencies:
        if (suggested_latency < info->defaultLowOutputLatency) {
            suggested_latency = info->defaultLowOutputLatency;
        }
    }

    actual_latency = (int) (suggested_latency * 1000 + 0.5);  // convert to ms
    // override with prefs file:
    actual_latency = prefs_latency(actual_latency);
    // override prefs with specified latency in open call
    actual_latency = (latency_ms >= 0 ? latency_ms : actual_latency);
    // override prefs in open call with GUI
    actual_latency = (arco_latency >= 0 ? arco_latency : actual_latency);
    arco_print("Audio latency = %d ms\n", actual_latency);
    suggested_latency = actual_latency * 0.001;

    input_params.device = actual_in_device;
    input_params.channelCount = actual_in_chans;
    input_params.sampleFormat = paFloat32;
    input_params.suggestedLatency = suggested_latency;
    input_params.hostApiSpecificStreamInfo = NULL;
    
    output_params.device = actual_out_device;
    output_params.channelCount = actual_out_chans;
    output_params.sampleFormat = paFloat32;
    output_params.suggestedLatency = suggested_latency;
    output_params.hostApiSpecificStreamInfo = NULL;
    
    close_request = false;
    actual_buffer_size = prefs_buffer_size(BL);
    actual_buffer_size = (buffer_size >= 0 ? buffer_size :
                          actual_buffer_size);
    actual_buffer_size = (arco_buffer_size >= 0 ? arco_buffer_size :
                          buffer_size);

    if (actual_buffer_size <= 0) {  // enforce minimum buffer_size
        actual_buffer_size = 32;
    }
    if (actual_buffer_size % BL) {  // enforce buffer_size multiple of BL
        actual_buffer_size = ((actual_buffer_size + (BL - 1)) / BL) * BL;
        arco_print("WARNING: audioio open buffer_size rounded up to %d\n",
                   actual_buffer_size);
    }
    if (actual_buffer_size > BL) {  // warn of big buffers (higher latency)
        arco_print("WARNING: audioio open buffer_size is %d\n",
                   actual_buffer_size);
    }
    if (actual_in_chans == 0 || actual_in_device == -1) {
        input_params_ptr = NULL;
    }
    if (actual_out_chans == 0 || actual_out_device == -1) {
        output_params_ptr = NULL;
    }
    if (!input_params_ptr && !output_params_ptr) {
        err = 1;
        goto done;
    }
    if (input_params_ptr) {
        if (request_record_status() != 3) {
            request_record_permission();  // ask for audio input
        }
        int status;
        while ((status = request_record_status()) == 0) {
            arco_print("Waiting for authorization...\n");
            sleep(1);
        }
        if (status != 3) {
            arco_warn("Audio input not authorized. Input will be zero.\n");
        }
    }

    err = Pa_OpenStream(&audio_stream, input_params_ptr, output_params_ptr,
                        AR, actual_buffer_size, paClipOff | paDitherOff, 
                        pa_callback, NULL);
    if (err != paNoError) goto done;
    
    aud_state = STARTING;
    
    // notify control service that audio is finally starting
    o2sm_send_start();
    strcpy(control_service_addr + control_service_addr_len, "starting");
    o2sm_send_finish(0.0, control_service_addr, true);

    err = Pa_StartStream(audio_stream);
    if (err != paNoError) {
        Pa_CloseStream(&audio_stream);  // ignore any close error
        goto done;  // report the error starting the stream
    }
    arco_print("audioio open completed\n");
done:
    arco_print("Requested input chans: %d\n", req_in_chans);
    arco_print("Requested output chans: %d\n", req_out_chans);
    arco_print("Actual device input chans: %d\n", actual_in_chans);
    arco_print("Actual device output chans: %d\n", actual_out_chans);
    arco_print("Input device: %d\n", actual_in_device);
    arco_print("Output device: %d\n", actual_out_device);
    const PaStreamInfo *stream_info = Pa_GetStreamInfo(audio_stream);
    if (stream_info) {
        arco_print("Reported input latency: %g\n", stream_info->inputLatency);
        arco_print("Reported output latency: %g\n", stream_info->outputLatency);
        arco_print("Reported sample rate: %g\n", stream_info->sampleRate);
    } else {
        arco_warn("Pa_GetStreamInfo returned NULL after audio open\n");
    }
    if (err < 0) {  // print a PortAudio error
         arco_error("Audio open error: %d, %s\n", err, Pa_GetErrorText(err));
    } else if (err == 1) {  // print configuration error
        arco_error("Audio open error: 0 input and 0 output channels\n");
    }
}
    

/* O2SM INTERFACE: /arco/close ;  --Close audio stream */
static void arco_close(O2SM_HANDLER_ARGS)
{
    if (aud_state == IDLE) return;
    close_request = true;
}


static void logfile_close(SNDFILE **file, char *name)
{
    if (*file) {
        sf_close(*file);
        *file = NULL;
        arco_print("Audio log file closed: %s\n", name);
    } else {
        arco_warn("Audio log file is not open: %s\n", name);
    }
}


// output log captures the intended number of channels (aud_out_chans)
//    even if the audio output is mixed down to a smaller actual_out_chans
//
static bool logfile_open(SNDFILE **file, char *name, char *init, int chans)
{
    SF_INFO info;
    // close if already open
    if (*file) {
        logfile_close(file, name);
    }
    // save (new) name:
    strncpy(name, init, ARCO_STRINGMAX);
    name[ARCO_STRINGMAX - 1] = 0;
    // open file:
    info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
    info.channels = chans;
    info.samplerate = AR;
    *file = sf_open(name, SFM_WRITE, &info);
    if (*file) {
        arco_print("Audio log file opened: %s\n", name);
    } else {
        arco_warn("Audio log file open failed: %s\n", name);
        return false;
    }
    return true;
}


/* O2SM INTERFACE: /arco/logaud
        int32 enable, int32 out, string filename;
  Log input or output.  enable is 1 or 0 to start or stop the logging.
  Starting overwrites the file. out is 0 to log input, 1 to log
  output. The filename should include a ".wav" extension. If enable is
  0 (false), filename is ignored, so "" is recommended.
*/
void arco_logaud(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32 enable = argv[0]->i;
    int32 out = argv[1]->i;
    char *filename = argv[2]->s;
    // end unpack message

    if (enable) {
        if (out) {
            log_output = logfile_open(&audiooutlogfile, audiooutlogfilename,
                                filename, ugen_table[PREV_OUTPUT_ID]->chans);
        } else {
            log_input = logfile_open(&audioinlogfile, audioinlogfilename,
                                     filename, ugen_table[INPUT_ID]->chans);
        }
    } else {
        if (out) {
            logfile_close(&audiooutlogfile, audiooutlogfilename);
            log_output = false;
        } else {
            logfile_close(&audioinlogfile, audioinlogfilename);
            log_input = false;
        }
    }
}


/* O2SM INTERFACE: /arco/load  
       int32 count;
   Impose a cpu load on the audio thread for testing.  Count is the
   number of extra function calls to make for every block of audio
   samples. If underflow occurs, the load is removed for the next 1000
   blocks.
*/
void arco_load(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32 count = argv[0]->i;
    // end unpack message

    cpuload = count;
}


// called to keep this zone running when there are no audio callbacks
// this can be called directly from the main O2 thread.
void arco_thread_poll()
{
    if (aud_state == RUNNING || aud_state == FIRST) return;
    if (aud_state == STARTED) {
        aud_state = FIRST;
        return;
    }
    
    if (aud_state == STOPPING) { // adjust offset
        aud_state = IDLE;
    }
    // now, aud_state == IDLE
    arco_wall_time = o2_native_time() + arco_wall_time_offset;

    O2_context *save_ctx = o2_ctx;
    o2_ctx = &aud_o2_ctx;
    o2sm_poll();
    // if audio is possibly running or going to run, we use this
    // request flag to cause it to stop. Otherwise, we can safely
    // (re)initialize everything.
    if (aud_reset_request &&
        (aud_state == UNINITIALIZED || aud_state == IDLE)) {
        aud_reset_request = false;  // clear the request
        output_set.clear();
        run_set.clear();
        for (int i = 0; i < ugen_table.size(); i++) {
            Ugen_ptr ugen = ugen_table[i];
            if (ugen) {
                ugen_table[i] = NULL;
                ugen->unref();
            }
        }
        actual_in_chans = 0;
        actual_out_chans = 0;

        // notify the control service that reset is finished
        o2sm_send_start();
        strcpy(control_service_addr + control_service_addr_len, "reset");
        o2sm_send_finish(0.0, control_service_addr, true);
    }

    // restore thread local context
    o2_ctx = save_ctx;
}


void audioio_initialize(Bridge_info *bridge)
{
    O2_context *save = o2_ctx;
    o2sm_initialize(&aud_o2_ctx, bridge);
    o2sm_service_new("arco", NULL);

    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/test1", "s", arco_test1, NULL, true, true);
    o2sm_method_new("/arco/prtree", "", arco_prtree, NULL, true, true);
    o2sm_method_new("/arco/reset", "s", arco_reset, NULL, true, true);
    o2sm_method_new("/arco/devinf", "s", arco_devinf, NULL, true, true);
    o2sm_method_new("/arco/output", "i", arco_output, NULL, true, true);
    o2sm_method_new("/arco/mute", "i", arco_mute, NULL, true, true);
    o2sm_method_new("/arco/open", "iiiis", arco_open, NULL, true, true);
    o2sm_method_new("/arco/close", "", arco_close, NULL, true, true);
    o2sm_method_new("/arco/logaud", "iis", arco_logaud, NULL, true, true);
    o2sm_method_new("/arco/load", "i", arco_load, NULL, true, true);
    // END INTERFACE INITIALIZATION

    Pa_Initialize();
    ugen_initialize();
    actual_in_chans = 0;  // no input open yet
    actual_out_chans = 0;  // no output open yet
    
    o2_ctx = save;  // restore caller (probably main O2 thread)'s context
    // now that o2_ctx is restored, we can set the clock source:
    o2_clock_set(arco_time, NULL);
}


// When a unit generator is deleted, we need to take it out of the run_set
// and/or output_set. Members of these sets have flags set, which cause
// this function to be called by Ugen::unref().
//
void aud_forget(int id)
{
    ANY_UGEN_FROM_ID(ugen, id, "aud_forget");
    if (ugen->flags & IN_OUTPUT_SET) {
        ugen->flags &= ~IN_OUTPUT_SET;
        forget_ugen_id(id, output_set);
    }
    if (ugen->flags & IN_RUN_SET) {
        ugen->flags &= ~IN_RUN_SET;
        forget_ugen_id(id, run_set);
    }
}
