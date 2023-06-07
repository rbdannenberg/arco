// Probe.cpp -- probe for audio-rate or control-rate signals
//
// Roger B. Dannenberg
// May 2023

#include "o2.h"
#include "arcougen.h"
#include "o2atomic.h"
#include "sharedmem.h"
#include "const.h"
#include "audioblock.h"
#include "probe.h"

const char *Probe_name = "Probe";


void Probe::real_run()
{
    if (state == PROBE_IDLE || inp == NULL) return;
    int inp_len = (inp->rate == 'a' ? BL : 1);

    inp_samps = inp->run(current_block);  // update input

    if (state == PROBE_DELAYING) { // wait for next period to start
        wait_count--;
        if (wait_count > 0) {
            return;
        }
        if (direction == 0) { // start collecting immediately
            goto start_collect;
        }
        state = PROBE_WAITING;
        wait_count = wait_in_blocks;
    }

    if (state == PROBE_WAITING) { // look for threshold crossing
        next = 0;
        if (direction > 0) {
            while (next < BL) {
                Sample s = inp_samps[next];
                if ((prev_sample < threshold) && (s >= threshold)) {
                    goto start_collect;
                }
                prev_sample = s;
                next++;
            }
        } else if (direction < 0) {
            while (next < BL) {
                Sample s = inp_samps[next];
                if ((prev_sample > threshold) && (s <= threshold)) {
                    goto start_collect;
                }
                prev_sample = s;
                next++;
            }
        } else {
            goto start_collect;
        }
        // invariant: next == BL
        // see if its time to do an auto sweep
        wait_count--; // if initially zero, don't auto sweep, so we
        // decrement before testing. If wait_count started out at 
        // zero, the following test will fail. This is a feature, not
        // a bug.
        if (wait_count == 0) {
            goto start_collect;
        }
        return;
    }
 collect:
    while (next < BL) {
        // copy the next frame from inp_samps. Since we claimed we have one
        // channel, inp_stride will be zero and not helpful, so we need to
        // use inp_len, a local variable computed above.
        for (int c = channel_offset; c < channel_offset + channels; c++) {
            *sample_ptr++ = inp_samps[next + inp_len * c];
        }
        frames_sent++;
        next += stride;
        if (sample_ptr >= sample_fence || frames_sent >= frames) {
            // we added the last frame for this message
            while (sample_ptr < sample_fence) {
                *sample_ptr++ = 0; // zero fill if there is room left over
            }
            msg->data.length = (int32_t) (((char *) sample_fence) -
                                          ((char *) &(msg->data.misc)));
            o2sm_message_send(msg);
            msg = NULL;
            // are we done yet, or do we need more messages?
            if (frames_sent < frames || period == 0) {
                // keep accumulating samples, but we need a new msg
                prepare_msg();
                if (frames_sent >= frames) {
                    frames_sent = 0;
                }
            } else if (period > 0) {
                state = PROBE_DELAYING;
                wait_count = (int) ((prev_start_block + delay_in_blocks) -
                                    current_block);
                if (wait_count < 1) wait_count = 1;
                return;
            } else /* if (period < 0) */ { // 1-shot: no more messages
                state = PROBE_IDLE;
                return;
            }
        }
    }
    next -= BL;
    return;
 start_collect:
    state = PROBE_COLLECTING;
    frames_sent = 0;
    prepare_msg();
    prev_start_block = current_block;
    goto collect;
}


/* O2SM INTERFACE: /arco/probe/new int32 id, int32 chans,
                         int32 inp_id, string reply_addr;
 */
void arco_probe_new(O2SM_HANDLER_ARGS)
{
    o2_msg_data_print(msg); putchar('\n');
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t inp_id = argv[1]->i;
    char *reply_addr = argv[2]->s;
    // end unpack message

    ANY_UGEN_FROM_ID(inp, inp_id, "arco_probe_new");
    new Probe(id, inp, reply_addr);
}


/* O2SM INTERFACE: /arco/probe/probe
       int32 id,
       float period,
       int32 frames,
       int32 chan,
       int32 nchans,
       int32 stride;
 */
void arco_probe_probe(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    float period = argv[1]->f;
    int32_t frames = argv[2]->i;
    int32_t chan = argv[3]->i;
    int32_t nchans = argv[4]->i;
    int32_t stride = argv[5]->i;
    // end unpack message

    UGEN_FROM_ID(Probe, probe, id, "arco_probe_probe");
    probe->probe(period, frames, chan, nchans, stride);
}


/* O2SM INTERFACE: /arco/probe/thresh
       int32 id,
       float threshold,
       int32 direction,
       float max_wait;
 */
void arco_probe_thresh(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    float threshold = argv[1]->f;
    int32_t direction = argv[2]->i;
    float max_wait = argv[3]->f;
    // end unpack message

    UGEN_FROM_ID(Probe, probe, id, "arco_probe_thresh");
    probe->thresh(threshold, direction, max_wait);
}


/* O2SM INTERFACE: /arco/probe/stop int32 id;
 */
void arco_probe_stop(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    // end unpack message

    UGEN_FROM_ID(Probe, probe, id, "arco_probe_stop");
    probe->stop();
}


static void probe_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/probe/new", "iis", arco_probe_new, NULL, true, true);
    o2sm_method_new("/arco/probe/probe", "ifiiii", arco_probe_probe, NULL, true, true);
    o2sm_method_new("/arco/probe/thresh", "ifif", arco_probe_thresh, NULL, true, true);
    o2sm_method_new("/arco/probe/stop", "i", arco_probe_stop, NULL, true, true);
    // END INTERFACE INITIALIZATION
}


Initializer probe_init_obj(probe_init);
