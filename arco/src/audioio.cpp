/* audioio.cpp -- audio dsp process for Arco
 *
 * Roger B. Dannenberg
 * Dec 2021
 */

/*

Internal States

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

1. Audio input has aud_in_chans channels. These are
de-interleaved and copied to the *output* of ugen_table[INPUT_ID].

2. ugen_table[INPUT_ID] is a Ugen of subclass Thru.  If there are too
many input channels, extras are ignored. (But initially, we only open
as many channels as we can use.) If there are too few, extra channels
are zero.

3. Audio graphs can reference ugen_table[INPUT_ID], which acts as a
source, to get samples.

4. run_set contains unit generators that must be run, but which are
not producing audio output, e.g., an audio feature computation. These
are computed after input is available.

5. output is taken from ugen_table[OUTPUT_ID], an Sum unit generator,
which sums an arbitrary number of inputs and provides a master gain
control. Unfortunately, we need an extra copy to get the samples into
PortAudio, but since PortAudio expects interleaved samples and may
have a different number of channels, we need to do a copy anyway.

7. The output is then interleaved to form true audio output. If the
output has a different number of channels, the following adaptation is
used:

8. Let aud_out_chans = arco_output->chans. If aud_out_chans >
actual_out_chans, the extra output channels are "wrapped" and summed
into the first aud_out_chans. E.g. if there are 6 output channels but
only 2 actual channels, channels 2 and 4 are added to 0, and channels
3 and 5 are added to 1, so everything is routed to channels 0 or 1. If
aud_out_chans < actual_out_chans, the extra channels are filled with
zeros.

External Control

See doc/design.md

*/
#include <unistd.h>
#include <stdio.h>
#include <algorithm>  // min
#ifdef __x86_64__
#include <xmmintrin.h>
#endif
#include <string.h>
#include <inttypes.h>
#include "sndfile.h"
#include "portaudio.h"
#include "prefs.h"
#include "arcoinit.h"
#include "arcougen.h"
#include "o2atomic.h"
#include "sharedmem.h"
#if defined(__APPLE__)
#include "recperm.h"
#endif
#include "prefs.h"
#include "sum.h"
#include "audioio.h"
#include "thru.h"


static const int ZERO_PAD_COUNT = 4410;  // how many zeros to write when closing

/* this is not used:
const char *aud_state_name[] = {
    "UNINITIALIZED", "IDLE", "STARTING", "STARTED",
    "FIRST", "RUNNING", "STOPPING" };
*/

using std::min;

// Note: audio processing can be carried out by more than one thread.
// When the audio callback happens, we set the thread_local o2_ctx
// variable to &aud_o2_ctx. When audio is not running, someone should
// call arco_thread_poll(), which saves o2_ctx on the stack, sets
// o2_ctx to &aud_o2_ctx, checks for messages and events, then restores
// o2_ctx.
O2_context aud_o2_ctx;  // O2 context for audio thread

O2queue *arco_msg_queue = NULL;

int aud_state = UNINITIALIZED;
int aud_quit_request = false;  // request to o2sm_finish() at a clean spot
bool aud_reset_request = false;
int64_t aud_frames_done = 0;
int aud_blocks_done = 0;
bool aud_heartbeat = true;
O2sm_info *audio_bridge = NULL;

// slew-rate limited master gain control:
float aud_gain = 1.0;
float aud_target_gain = 1.0;
const float aud_gain_factor = 0.03;

// max slew rate is full-scale 0 to 1 in 100 msec at 48kHz.
// this seems really slow, but at 20 msec, moving a GUI slider and
// generating 5 or 10 updates/second created an audible stairstep effect
// even though an instantaneous jump sounded perfectly smooth.
// (It will change with sample rate, but exact value does not matter.)
const float aud_max_gain_incr = 1.0 / (0.1 * 48000);

static bool arco_thread_exited = false;  // stop processing after o2sm_finish()
static int actual_in_chans = -1;     // number of channels in callback
static int actual_out_chans = -1;    // number of channels in callback
static int actual_latency_ms = -1;
static int actual_buffer_size = BL;
static int actual_in_id = -1;
static int actual_out_id = -1;

static int aud_close_request;
static int aud_zero_fill_count = 0;
static PaStream *audio_stream;

static Vec<Ugen_ptr> run_set;  // UG ID's to refresh every block
static Vec<Ugen_ptr> output_set;  // list of UG ID's to sum to form output

static int cpuload = 0;

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


static bool forget_ugen(Ugen_ptr ugen, Vec<Ugen_ptr> &set)
{
    for (int i = 0; i < set.size(); i++) {
        if (set[i] == ugen) {
            /*
            printf("forget_ugen: removing %p (%i) at %i from %p\n",
                   ugen, ugen->id, i, &set);
            printf("    size %d contains", set.size());
            for (int j = 0; j < set.size(); j++) printf(" %d", set[j]->id);
            */
            set.remove(i);
            ugen->unref();
            /*
            printf("    after remove, size %d contains", set.size());
            for (int j = 0; j < set.size(); j++) printf(" %d", set[j]->id);
            printf("\n");
            */
            return true;
        }
    }
    return false;
}


/* O2SM INTERFACE: /arco/hb int32 hb; -- enable/disable printing block counts */
static void arco_hb(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t hb = argv[0]->i;
    // end unpack message

    printf("Block count printing %s\n", hb ? "enabled" : "disabled");
    aud_heartbeat = (hb != 0);
}


/* O2SM INTERFACE: /arco/test1 string s;
   To test if messages are being received, this function
   will print the string on receipt (followed by newline)
*/
static void arco_test1(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    char *s = argv[0]->s;
    // end unpack message

    printf("%s\n", s);
}


/* O2SM INTERFACE: /arco/prtree ;
   Print a tree of all unit generators
*/
static void arco_prtree(O2SM_HANDLER_ARGS)
{
    arco_print("------------ Ugen Tree ----------------\n");
    UGEN_FROM_ID(Sum, sum, OUTPUT_ID, "arco_prtree");
    sum->print_tree(0, true, "audio output sum");
    for (int i = 0; i < run_set.size(); i++) {
        Ugen_ptr ugen = run_set[i];
        if (ugen->flags & UGEN_MARK) { // special case: print again
            ugen->flags &= ~UGEN_MARK;
        }
        ugen->print_tree(0, true, "member of run_set");
    }

    // clear the marks by traversing tree again
    for (int i = 0; i < sum->inputs.size(); i++) {
        Ugen_ptr ugen = sum->inputs[i];
        ugen->print_tree(0, false, "");
    }
    for (int i = 0; i < run_set.size(); i++) {
        Ugen_ptr ugen = run_set[i];
        ugen->print_tree(0, false, "");
    }
    arco_print("---------------------------------------\n");
}


/* O2SM INTERFACE:  /arco/prugen int32 id string param; 
   -- print one Ugen to console
 */
static void arco_prugen(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    const char *param = argv[1]->s;
    // end unpack message

    ANY_UGEN_FROM_ID(ugen, id, "arco_output");
    ugen->print(0, param);
}


/* O2SM INTERFACE: /arco/prugens ;  -- print all ugens */
static void arco_prugens(O2SM_HANDLER_ARGS)
{
    arco_print("------------ Ugen List ----------------\n");
    for (int i = 0; i < ugen_table.size(); i++) {
        Ugen_ptr ugen = ugen_table[i];
        if (ugen) {
            ugen->print(1);
        }
    }
    arco_print("---------------------------------------\n");
}



/* O2SM INTERFACE: /arco/reset string s;
   Reset the server: stop audio, disconnect run set and outputs,
   send to /<s>/reset, where <s> is the control service name
   passed as an argument of this message.
*/
static void arco_reset(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    char *s = argv[0]->s;
    // end unpack message

    set_control_service(s);
    aud_close_request = true;
    aud_zero_fill_count = 0;
    aud_reset_request = true;
}


/* O2SM INTERFACE: /arco/quit;
   Arco is quitting -- shut down the whole application, audio first
*/
static void arco_quit(O2SM_HANDLER_ARGS)
{
    o2sm_send_cmd("/fileio/quit", 0, "");
    aud_close_request = true;
    aud_reset_request = true;
    aud_quit_request = true;
}


// req_in_chans -- get the number of input channels to request
int req_in_chans(int in_chans)
{
    return (in_chans >= 0 ? in_chans :
            (ugen_table[INPUT_ID] ? ugen_table[INPUT_ID]->chans : 0));
}

// req_out_chans -- get the number of input channels to request
int req_out_chans(int out_chans)
{
    return (out_chans >= 0 ? out_chans :
            (ugen_table[OUTPUT_ID] ?
             ugen_table[OUTPUT_ID]->chans : 0));
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
        block_zero_n(dest, dest_chans - src_chans);
    }
}


// mix from arco output to PortAudio output. Output channels are sum
// of input channels mod actual_out_chans, e.g. 0+2->0, 1+3->1
//
void mix_down_interleaved(Sample_ptr dst, int actual_out_chans,
                          Sample_ptr src, int arco_out_chans)
{
    if (actual_out_chans == 0) {
        return;  // if no real output, no place or need to mix any samples
    }
    // Note: arco_out_chans > actual_out_chans > 0, and
    //       src is bigger than dst
    // Begin by interleaving the first actual_out_chans:
    Sample_ptr dst_ptr = dst;
    for (int i = 0; i < BL; i++) {  // outer loop is over output frames
        Sample_ptr src_ptr = src + i;
        // copy actual_out_chans frames
        for (int j = 0; j < actual_out_chans; j++) {  // within each frame
            *dst_ptr++ = *src_ptr;
            src_ptr += BL;  // skip to next channel
        }
    }
    // Sum the rest, actual_out_chans at a time, until all channels are output
    for (int c = actual_out_chans; c < arco_out_chans;
         c += actual_out_chans) {
        // mix up to next actual_out_chans into first actual_out_chans
        // but if there are not actual_out_chans left, mix how many
        // are left:
        int howmany = min(actual_out_chans, arco_out_chans - c);
        Sample_ptr src_base_ptr = src + c * BL;
        Sample_ptr dst_frame_ptr = dst;
        for (int i = 0; i < BL; i++) {  // loop over output frames
            Sample_ptr src_ptr = src_base_ptr + i;
            // copy howmany frames
            Sample_ptr dst_ptr = dst_frame_ptr;
            for (int j = 0; j < howmany; j++) { // first howmany channels
                *dst_ptr++ += *src_ptr;         // in each frame
                src_ptr += BL;
            }
            dst_frame_ptr += actual_out_chans;  // skip to next frame
        }
    }
}
    
    
// add channels to buffer while copying to output. Output is to be
// interleaved, but source is not
//
void copy_interleaved_with_zero_fill(Sample_ptr dst, int actual_out_chans,
                                     Sample_ptr src, int arco_out_chans)
{
    for (int i = 0; i < BL; i++) {  // copy BL frames
        int c;
        Sample_ptr src_ptr = src + i;  // offset by frame, stride is BL
        for (c = 0; c < arco_out_chans; c++) {  // copy existing channels
            *dst++ = *src_ptr;
            src_ptr += BL;
        }
        while (c++ < actual_out_chans) {  // zero fill 
            *dst++ = 0.0F;
        }
    }
}


// called for each block of BL * actual_in_chans samples input is
// interleaved, output is computed with interleaving But input is
// de-interleaved and copied to the first actual_in_chans of
// aud_input_ug. For output, we compute BL * arco_out_chans. If we
// have enough actual channels, then we interleave to output.  If we
// don't have enough actual samples, we allocate a buffer and
// interleave there in a separate interleave step.  Then to form
// output, we have to mix channels.
//
static int callback_entry(float *input, float *output,
                          PaStreamCallbackFlags flags)
{
    o2_ctx = &aud_o2_ctx;  // make thread context available to o2 functions
    
    if (aud_state == RUNNING && !aud_close_request) {
        // arco time will suffer from audio computation time jitter, and
        // if PortAudio blocks are large (e.g. multiple Arco blocks), time
        // will tend to advance in big steps on each callback.
        arco_wall_time += BP; // time is advanced by block period
    } else {
        block_zero_n(output, actual_out_chans);
        if (aud_close_request) {
            if (aud_zero_fill_count >= ZERO_PAD_COUNT) {
                 return paComplete;
            }
            aud_zero_fill_count += BL;
        }
        return paContinue;
    }
    // we are RUNNING on the callback
#ifdef  __x86_64__
    _mm_setcsr(_mm_getcsr() | 0x8040); // SSE Denormals handling: At least
    // some Intel processors are *extremely* slow handling denormals
    // which can occur with exponential decay of audio to
    // silence. Rounding to zero is perfectly acceptable for audio and
    // avoids spikes in computation that cause processing to be slower
    // than real time. This enables denormals-are-zero (DAZ) and
    // flush-to-zero (FTZ) flags for SSE.
#endif

    // Impose artificial load, but stop for a second if underflows happen.
    // I don't think the optimizer will remove a function call.
    if (cpuload && underflow_count > 1000) {
        for (int i = 0; i < cpuload; i++) busywork(i);
    }

    /*
    if (blocks_done % 1000 == 0 && actual_in_chans > 0) {
        arco_print("Audio callback %ld input %g\n", blocks_done, *input);
    }
    */

    o2sm_poll();  // called here to get block-accurate timing
    aud_frames_done += BL;
    // If main program is shutting down due to an error, it could free the
    // audio thread's memory. That shouldn't happen, but did once, so this
    // safeguard stops the audio thread before more damage occurs.
    if (!ugen_table.bounds_check(INPUT_ID)) {
        return paAbort;
    }
    Ugen_ptr input_ug = ugen_table[INPUT_ID];
    Sample_ptr aout;
    aud_blocks_done++;  // bring everyone up to this count
    if (input_ug && (aout = &input_ug->output[0])) {
        copy_deinterleave(aout, input_ug->chans, input, actual_in_chans);
        if (((Thru *) input_ug)->alternate == NULL) {
            input_ug->set_current_block(aud_blocks_done);
        }  // otherwise, the input is overwritten by alternate, and we must
           // not update current_block so run will call real_run.
    }

    for (int i = 0; i < run_set.size(); i++) {
        // since our current_block was advanced after input was read
        // it is the number we want
        Ugen_ptr ug = run_set[i];
        if (ug) {
            ug->run(aud_blocks_done);
        }
    }

    Ugen_ptr arco_output = ugen_table[OUTPUT_ID];
    // The commented conditions will stop computation of audio output
    // when there is no real output stream to write to. It seems
    // reasonable not to compute audio when it is not needed, but maybe
    // the intended output is monitored by some analysis that generates some
    // outgoing O2 messages, or maybe if output is not processed, then
    // sounds are created but never terminate, exhausting the available
    // Ugen namespace, or maybe we are only interested in getting a log
    // of the computed audio. Since failing to output sound could have side
    // effects like this, we always compute output even when there is no
    // stream to send it to.
    //     Probably, it is a bug not to have arco_output, and it should
    // be created automatically, and probably it is OK for arco_output
    // to have zero channels, but can we specify that? At least there is
    // code below (see else part) to output zeros when Arco produces no
    // output but the audio device expects output.
    if (!arco_output) {
        printf("WARNING (Arco audio callback): arco_output is NULL\n");
    } else if (arco_output->chans > 0) {    // Compute the output:
        Sample_ptr arco_out_samps = arco_output->run(aud_blocks_done);
        int arco_out_chans = arco_output->chans;
        // now we need actual_out_chans of interleaved samples in output
        // case 1: arco_output chans > actual output chans.
        // case 2: actual output chans is bigger than graph output,
        //    so interleave with zero fill
        // case 3: arco_output chans = actual output chans; just interleave.
        // do we need to mix down channels for output?
        if (arco_out_chans > actual_out_chans) { // yes - case 1
            mix_down_interleaved(output, actual_out_chans,
                                 arco_out_samps, arco_out_chans);
        } else if (arco_out_chans < actual_out_chans) {  // case 2
            copy_interleaved_with_zero_fill(output, actual_out_chans,
                                            arco_out_samps, arco_out_chans);
        } else {  // arco_out_chans == actual_out_chans
            // interleave the computed output back to buffer
            transpose(output, arco_output->out_samps, arco_out_chans, BL);
        }
        // apply final gain control. Note that channels are interleaved.
        // simple lowpass filter computes new gain after BL samples:
        float gain_next = aud_gain +
                          (aud_target_gain - aud_gain) * aud_gain_factor;
        // linear interpolation from one block to the next:
        float gain_incr = (gain_next - aud_gain) * BL_RECIP;
        Sample_ptr p = output;
        for (int i = 0; i < BL; i++) {
            for (int ch = 0; ch < actual_out_chans; ch++) {
                *p++ *= aud_gain;
            }
            aud_gain += gain_incr;
        }
    } else if (actual_out_chans > 0) {  // no synthesized output, so zero output
        block_zero_n(output, actual_out_chans);
    }

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
    if (aud_heartbeat && aud_blocks_done && aud_blocks_done % 1000 == 0) {
        printf("%d blocks\n", aud_blocks_done);
    }
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
            // if any block returns non-paContinue, remember it
            if (r != paContinue) result = r;
        }
        if (result != paContinue) {
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


/* O2SM INTERFACE:  /arco/run int32 id; -- add Ugen to run set */
static void arco_run(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    // end unpack message

    ANY_UGEN_FROM_ID(ugen, id, "arco_run");

    // make sure this is not already in run set
    if (ugen->flags & IN_RUN_SET) {
        arco_warn("/arco/run %d @ %p already in run set\n", id, ugen);
        return;
    }
    ugen->flags |= IN_RUN_SET;
    run_set.push_back(ugen);
    ugen->ref();
    // printf("arco_run %d @ %p inserted, flags %x\n", id, ugen, ugen->flags);
}


/* O2SM INTERFACE:  /arco/unrun int32 id; -- remove Ugen from run set */
static void arco_unrun(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    // end unpack message

    ANY_UGEN_FROM_ID(ugen, id, "arco_unrun");
    if (ugen && (ugen->flags & IN_RUN_SET)) {
        forget_ugen(ugen, run_set);
        ugen->flags &= ~IN_RUN_SET;
        /* printf("arco_unrun %d @ %p removed, flags %x\n",
                  id, ugen, ugen->flags); */
    } else {
        arco_warn("/arco/unrun %d not in run set, ignored\n", id);
    }
}


/* O2SM INTERFACE:  /arco/open
       int32 in_id, int32 out_id, 
       int32 in_chans, int32 out_chans,
       int32 latency_ms, int32 buffer_size,
       string ctrlservice;
   Open audio device(s) for input and output and begin processing
   audio.
*/
static void arco_open(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t in_id = argv[0]->i;
    int32_t out_id = argv[1]->i;
    int32_t in_chans = argv[2]->i;
    int32_t out_chans = argv[3]->i;
    int32_t latency_ms = argv[4]->i;
    int32_t buffer_size = argv[5]->i;
    char *ctrlservice = argv[6]->s;
    // end unpack message

    PaError err;
    PaDeviceIndex num_devices = 0;
    PaTime suggested_latency;
    PaStreamParameters input_params;
    PaStreamParameters *input_params_ptr = &input_params;
    PaStreamParameters output_params;
    PaStreamParameters *output_params_ptr = &output_params;

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

    actual_in_id = Pa_GetDefaultInputDevice();
    actual_out_id = Pa_GetDefaultOutputDevice();

    // /arco/open device specs override preferences
    if (in_id >= 0) actual_in_id = in_id;
    if (out_id >= 0) actual_out_id = out_id;
    
    suggested_latency = 0;
    in_chans = req_in_chans(in_chans);
    if (in_chans == 0) {
        actual_in_id = paNoDevice;
        arco_print("Zero input channels, so no input device to open.\n");
        actual_in_chans = 0;
    } else {
        const PaDeviceInfo *info = Pa_GetDeviceInfo(actual_in_id);
        arco_print("Aura selects input device: %s\n", info->name);
        actual_in_chans = in_chans;
        if (info->maxInputChannels < in_chans) {
            arco_print("\n    WARNING: only %d input channels available.\n",
                       info->maxInputChannels);
            actual_in_chans = info->maxInputChannels;
        }
        suggested_latency = info->defaultLowInputLatency;
    }


    out_chans = req_out_chans(out_chans);
    if (out_chans == 0) {
        actual_out_id = paNoDevice;
        arco_print("Zero output channels, so no output device to open.\n");
        actual_out_chans = 0;
    } else {
        const PaDeviceInfo *info = Pa_GetDeviceInfo(actual_out_id);
        arco_print("Aura selects output device: %s\n", info->name);
        actual_out_chans = out_chans;
        if (info->maxOutputChannels < out_chans) {
            arco_print("\n    WARNING: only %d output channels available.\n",
                       info->maxOutputChannels);
            actual_out_chans = info->maxOutputChannels;
        }
        // get max of suggested input and output latencies:
        if (suggested_latency < info->defaultLowOutputLatency) {
            suggested_latency = info->defaultLowOutputLatency;
        }
    }

    actual_latency_ms = (int) (suggested_latency * 1000 + 0.5);  // convert to ms
    // override prefs with specified latency in open call
    actual_latency_ms = (latency_ms >= 0 ? latency_ms : actual_latency_ms);
    arco_print("Audio latency = %d ms\n", actual_latency_ms);
    suggested_latency = actual_latency_ms * 0.001;

    input_params.device = actual_in_id;
    input_params.channelCount = actual_in_chans;
    input_params.sampleFormat = paFloat32;
    input_params.suggestedLatency = suggested_latency;
    input_params.hostApiSpecificStreamInfo = NULL;
    
    output_params.device = actual_out_id;
    output_params.channelCount = actual_out_chans;
    output_params.sampleFormat = paFloat32;
    output_params.suggestedLatency = suggested_latency;
    output_params.hostApiSpecificStreamInfo = NULL;
    
    aud_close_request = false;

    actual_buffer_size = (buffer_size >= 0 ? buffer_size : BL);
    if (actual_buffer_size <= 0) {  // enforce minimum buffer_size
        actual_buffer_size = BL;
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
    if (actual_in_chans == 0 || actual_in_id == -1) {
        input_params_ptr = NULL;
    }
    if (actual_out_chans == 0 || actual_out_id == -1) {
        output_params_ptr = NULL;
    }
    if (!input_params_ptr && !output_params_ptr) {
        err = 1;
        goto done;
    }
    if (input_params_ptr) {
#if defined(__APPLE__)
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
#endif
    }

    err = Pa_OpenStream(&audio_stream, input_params_ptr, output_params_ptr,
                        AR, actual_buffer_size, paClipOff | paDitherOff, 
                        pa_callback, NULL);
  done:
    if (err != paNoError) {    // failure is indicated by both 
        actual_in_chans = 0;   // actual_in_chans == 0 and 
        actual_out_chans = 0;  // actual_out_chans == 0
    } else {
        aud_state = STARTING;
    }
    // notify control service that audio is finally starting (or failed)
    o2sm_send_start();
    strcpy(control_service_addr + control_service_addr_len, "starting");
    o2sm_add_int32(actual_in_id);
    o2sm_add_int32(actual_out_id);
    o2sm_add_int32(actual_in_chans);
    o2sm_add_int32(actual_out_chans);
    o2sm_add_int32(actual_latency_ms);
    o2sm_add_int32(actual_buffer_size);
    o2sm_send_finish(0.0, control_service_addr, true);

    if (err == paNoError &&
        (err = Pa_StartStream(audio_stream)) != paNoError) {
        Pa_CloseStream(&audio_stream);  // ignore any close error
    } else {
        arco_print("audioio open completed\n");
    }

    arco_print("Requested input chans: %d\n", req_in_chans(in_chans));
    arco_print("Requested output chans: %d\n", req_out_chans(out_chans));
    arco_print("Actual device input chans: %d\n", actual_in_chans);
    arco_print("Actual device output chans: %d\n", actual_out_chans);
    arco_print("Input device: %d\n", actual_in_id);
    arco_print("Output device: %d\n", actual_out_id);
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
    aud_close_request = true;
    aud_zero_fill_count = 0;
}


/* O2SM INTERFACE: /arco/cpu; -- get the estimated CPU load */
void arco_cpu(O2SM_HANDLER_ARGS)
{
    float load = 0;
    if (audio_stream) {
        load = Pa_GetStreamCpuLoad(audio_stream);
    }
    o2sm_send_start();
    o2sm_add_float(load);
    strcpy(control_service_addr + control_service_addr_len, "cpu");
    o2sm_send_finish(0.0, control_service_addr, true);
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
    int32_t count = argv[0]->i;
    // end unpack message

    cpuload = count;
}

/* O2SM INTERFACE: /arco/gain float gain; -- set the master gain */
void arco_gain(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    float gain = argv[0]->f;
    // end unpack message

    if (aud_state == IDLE) {
        aud_gain = gain;  // so we start audio stream with correct gain
    }
    aud_target_gain = gain;
}


void arco_free_all_ugens()
{
    output_set.clear();
    run_set.clear();
    for (int i = 0; i < ugen_table.size(); i++) {
        Ugen_ptr ugen = ugen_table[i];
        if (ugen) {
            ugen->flags &= ~IN_RUN_SET;
            ugen_table[i] = NULL;
            ugen->unref();
        }
    }
}

// called to keep this zone running when there are no audio callbacks
// this can be called directly from the main O2 thread.
void arco_thread_poll()
{
    if (arco_thread_exited) {
        return;
    }
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
        arco_free_all_ugens();
        // notify the control service that reset is finished
        o2sm_send_start();
        strcpy(control_service_addr + control_service_addr_len, "reset");
        o2sm_send_finish(0.0, control_service_addr, true);
    }
    // we're IDLE now, so if quit is requested, do it now:
    if (aud_quit_request) {
        arco_free_all_ugens();
        ugen_table.finish();
        
        assert(run_set.size() == 0);
        run_set.finish();
        
        assert(output_set.size() == 0);
        output_set.finish();

        o2sm_finish();
        arco_thread_exited = true;  // stop further polling of audio
    }
    // restore thread local context
    o2_ctx = save_ctx;
}


// to be called from the main thread - creates audio shared memory thread,
// except the "thread" is provided by both a polling function called from
// the main thread and PortAudio callbacks (when PortAudio is actually
// streaming audio and after proper handshakes to hand off control of this
// "thread" from the main thread. The main thread regains control and 
// continues polling when a PortAudio stream ends.
void audioio_initialize()
{
    int rslt = o2_shmem_initialize();
    // it is ok if some other shared memory action has already started
    // the shared memory bridge in O2:
    assert(rslt == O2_SUCCESS || rslt == O2_ALREADY_RUNNING);
    assert(audio_bridge == NULL);  // not OK if this bridge was already created
    audio_bridge = o2_shmem_inst_new();  // make our bridge

    O2_context *save = o2_ctx;
    o2sm_initialize(&aud_o2_ctx, audio_bridge);

    o2sm_service_new("arco", NULL);

    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/hb", "i", arco_hb, NULL, true, true);
    o2sm_method_new("/arco/test1", "s", arco_test1, NULL, true, true);
    o2sm_method_new("/arco/prtree", "", arco_prtree, NULL, true, true);
    o2sm_method_new("/arco/prugens", "", arco_prugens, NULL, true, true);
    o2sm_method_new("/arco/prugen", "is", arco_prugen, NULL, true, true);
    o2sm_method_new("/arco/reset", "s", arco_reset, NULL, true, true);
    o2sm_method_new("/arco/quit", "", arco_quit, NULL, true, true);
    o2sm_method_new("/arco/devinf", "s", arco_devinf, NULL, true, true);
    o2sm_method_new("/arco/run", "i", arco_run, NULL, true, true);
    o2sm_method_new("/arco/unrun", "i", arco_unrun, NULL, true, true);
    o2sm_method_new("/arco/open", "iiiiiis", arco_open, NULL, true, true);
    o2sm_method_new("/arco/close", "", arco_close, NULL, true, true);
    o2sm_method_new("/arco/cpu", "", arco_cpu, NULL, true, true);
    o2sm_method_new("/arco/load", "i", arco_load, NULL, true, true);
    o2sm_method_new("/arco/gain", "f", arco_gain, NULL, true, true);
    // END INTERFACE INITIALIZATION

    // this one is manually inserted. It is implemented in ugen.cpp:
    o2sm_method_new("/arco/term", "if", arco_term, NULL, true, true);

    Pa_Initialize();
    ugen_initialize();
//    actual_in_chans = 0;  // no input open yet
//    actual_out_chans = 0;  // no output open yet
    
    o2_ctx = save;  // restore caller (probably main O2 thread)'s context
    // now that o2_ctx is restored, we can set the clock source:
    o2_clock_set(arco_time, NULL);
}

