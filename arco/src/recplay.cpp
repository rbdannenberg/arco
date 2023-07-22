/* recplay -- unit generator for arco
 *
 * based on irecplay.cpp from AuraRT
 *
 * Roger B. Dannenberg
 * Apr 2023
 */

#include "arcougen.h"
#include "recplay.h"

#define LOG2_BLOCKS_PER_BUFFER 10
#define LOG2_SAMPLES_PER_BUFFER (LOG2_BLOCKS_PER_BUFFER + LOG2_BL)
#define SAMPLES_PER_BUFFER (1 << LOG2_SAMPLES_PER_BUFFER)
#define INDEX_TO_BUFFER(i) ((i) >> LOG2_SAMPLES_PER_BUFFER)
#define INDEX_TO_OFFSET(i) ((i) & (SAMPLES_PER_BUFFER - 1))

// the last sample must be obtained from the owner of the samples
#define END_COUNT (lender_ptr ? lender_ptr->sample_count : sample_count)
// similarly, if we're borrowing samples, consult owner for recording status
#define RECORDING (lender_ptr ? lender_ptr->recording : recording)


// note table really goes from 0 to 1 over index range 2 to 102
// there are 2 extra samples at either end to allow for interpolation and
// off-by-one errors
float raised_cosine[COS_TABLE_SIZE + 5] = { 
    0, 0, 0, 0.00024672, 0.000986636, 0.00221902,
    0.00394265, 0.00615583, 0.00885637, 0.0120416, 0.0157084,
    0.0198532, 0.0244717, 0.0295596, 0.0351118, 0.0411227, 0.0475865,
    0.0544967, 0.0618467, 0.069629, 0.077836, 0.0864597, 0.0954915,
    0.104922, 0.114743, 0.124944, 0.135516, 0.146447, 0.157726,
    0.169344, 0.181288, 0.193546, 0.206107, 0.218958, 0.232087,
    0.245479, 0.259123, 0.273005, 0.28711, 0.301426, 0.315938,
    0.330631, 0.345492, 0.360504, 0.375655, 0.390928, 0.406309,
    0.421783, 0.437333, 0.452946, 0.468605, 0.484295, 0.5, 0.515705,
    0.531395, 0.547054, 0.562667, 0.578217, 0.593691, 0.609072,
    0.624345, 0.639496, 0.654508, 0.669369, 0.684062, 0.698574,
    0.71289, 0.726995, 0.740877, 0.754521, 0.767913, 0.781042,
    0.793893, 0.806454, 0.818712, 0.830656, 0.842274, 0.853553,
    0.864484, 0.875056, 0.885257, 0.895078, 0.904508, 0.91354,
    0.922164, 0.930371, 0.938153, 0.945503, 0.952414, 0.958877,
    0.964888, 0.97044, 0.975528, 0.980147, 0.984292, 0.987958,
    0.991144, 0.993844, 0.996057, 0.997781, 0.999013, 0.999753, 1, 1, 1};

const char *Recplay_name = "Recplay";

void Recplay::run_channel(int chan) {
    if (recording) {
        int buffer = (int) INDEX_TO_BUFFER(rec_index);
        int offset = (int) INDEX_TO_OFFSET(rec_index);

        // make sure we have space
        if (buffer >= my_buffers.size()) {
            assert(buffer == my_buffers.size());
            Sample_ptr b = O2_MALLOCNT(SAMPLES_PER_BUFFER * chans, Sample);
            assert(b);
            my_buffers.push_back(b);
        }
        memcpy(my_buffers[buffer] + offset, inp_samps, sizeof(Sample) * BL);
        rec_index += BL;
        if (rec_index > sample_count) sample_count = rec_index;
    }

    if (playing) {
        // fade out if we are near the end. This code works on block
        // boundaries so it may fade up to one block early (<1ms)
        // the test for !stopping is redundant but helps with debugging by
        // not calling stop multiple times. If we are recording, the 
        // END_COUNT is increasing, so we do not stop. (But this could
        // be bad if speed > 1 and we overtake the live recording. In that
        // case playback will stop abruptly.)
        //
        // "+ 1 + BL" is correct: round up fade_len * speed,
        // and then we need extra for interpolation.
        // note that fade_len is derived from fade when playback is
        // started. You cannot change the fade-out time after you start.
        if (!stopping && !RECORDING &&
            play_index + long(fade_len * speed) + 1 + BL >= END_COUNT) {
            stop();
        }

        // if the record buffer is very short, or if speed has changed,
        // it's possible that there is not enough time to fade out, 
        // in which case we could overrun the buffers and try to 
        // access non-existent memory. Test for overrun now, and just
        // stop processing if we're out of samples.
        // 
        // To continue, we need BL samples, so the final sample phase
        // will be:
        double final_phase = play_phase + (BL - 1) * speed;
        // With interpolation, we need one sample beyond final_phase
        // (and to keep this simple, we'll always arrange for fades to
        // end one sample early so that if we round up, we'll still be
        // in bounds.)
        long last_offset = long(final_phase + 1);
        // Now we can see if we're going to run out of samples:
        if (last_offset >= END_COUNT) {
            // Note that in some cases we might run out of samples in the
            // middle of a buffer, so we could actually output a few samples.
            // Thus, we might drop a fraction of a block of recorded samples.
            // The extra code to handle this does not seem worth the effort.
            // If the recording time is reasonable and if the speed is not
            // changing, this code will never be reached because the fadeout
            // will finish before we run out of samples.
            memset(out_samps, 0, sizeof(Sample) * BL);
            out_samps += BL;
            playing = false;
            arco_warn("Recplay: sudden stop, out of samples");
            send_action_id(action_id);
            return;
        }

        int buffer = (int) INDEX_TO_BUFFER(play_index);
        long offset = INDEX_TO_OFFSET(play_index);
        // phase is exact offset within buffer. Computed by subtracting
        // off the length of the previous buffers, which is play_index
        // minus offset, which is where we are in the current buffer.
        double phase = play_phase - (play_index - offset);

        int i;
        Sample *src = (*buffers)[buffer] + offset;
        if (fading) {
            if (speed == 1.0) {
                for (i = 0; i < BL; i++) {
                    long iphase = (long) fade_phase; // truncate to integer
                    float fraction = fade_phase - iphase; // get fraction 
                    // interpolated envelope
                    float env = raised_cosine[iphase] * (1 - fraction) +
                                raised_cosine[iphase + 1] * fraction;
                    // compute a sample
                    *out_samps++ = *src++ * env * *gain_samps;
                    fade_phase += fade_incr;
                    offset++;
                    if (offset >= SAMPLES_PER_BUFFER) {
                        offset = 0;
                        src = (*buffers)[++buffer];
                    }
                }
                play_index += BL;
                play_phase = play_index; // keep play_phase up-to-date in case
                                         // speed changes
            } else { // speed is not 1, so do linear interpolation
                for (i = 0; i < BL; i++) {
                    long iphase = (long) fade_phase; // truncate to integer
                    float fraction = fade_phase - iphase; // get fraction 
                    // interpolated envelope
                    float env = raised_cosine[iphase] * (1 - fraction) +
                                raised_cosine[iphase + 1] * fraction;
                    // compute a sample. frac is the fraction of a sample
                    // between the current position (play_phase) and the 
                    // location of the next sample (play_index)
                    fraction = play_index - play_phase;
                    Sample next_sample = *src;
                    Sample samp = prev_sample * fraction +
                                  next_sample * (1 - fraction);
                    *out_samps++ = samp * env * *gain_samps;
                    fade_phase += fade_incr;
                    phase += speed;
                    play_phase += speed;
                    // if speed > 1, we can advance past more than one sample
                    // We'll do this the slow way -- one sample at a time.
                    while (phase > offset) { // advance to the next sample
                        prev_sample = next_sample;
                        offset++;
                        play_index++;
                        src++;
                        if (offset >= SAMPLES_PER_BUFFER) {
                            offset = 0;
                            phase -= SAMPLES_PER_BUFFER;
                            src = (*buffers)[++buffer];
                        }
                    }
                }
            }
            fade_count--;
            if (fade_count <= 0) {
                fading = false;
                //LINEAR if (incr < 0.0) {
                if (stopping) { // stopping means fading out, not fading in
                    stopping = false;
                    playing = false;
                    if (loop) {
                        start(start_time);
                    } else {
                        send_action_id(action_id);
                    }
                } // else we finished fading in
            }
        // before starting, see if we're at the end of the samples
        } else {
            if (speed == 1.0) { // fast copy if no interpolation
                for (i = 0; i < BL; i++) {
                    // compute a sample
                    *out_samps++ = *src++ * *gain_samps;
                    offset++;
                    if (offset >= SAMPLES_PER_BUFFER) {
                        offset = 0;
                        src = (*buffers)[++buffer];
                    }
                }
                play_index += BL;
                play_phase = play_index; // keep play_phase up-to-date in case
                                         // speed changes
            } else { // slow: interpolation because speed != 1.0
                for (i = 0; i < BL; i++) {
                    // compute a sample
                    Sample frac = play_index - play_phase;
                    Sample next_sample = *src;
                    Sample samp = prev_sample * frac + next_sample * (1 - frac);
                    *out_samps++ = samp * *gain_samps;
                    phase += speed;
                    play_phase += speed;
                    // if speed > 1, we can advance past more than one sample.
                    // We'll do this the slow way -- one sample at a time.
                    while (phase > offset) { // advance to the next sample
                        prev_sample = next_sample;
                        offset++;
                        play_index++;
                        src++;
                        if (offset >= SAMPLES_PER_BUFFER) {
                            offset = 0;
                            phase -= SAMPLES_PER_BUFFER;
                            src = (*buffers)[++buffer];
                        }
                    }
                }
            }
        }
    } else { // !playing, so just output zeros
        memset(out_samps, 0, sizeof(Sample) * BL);
        out_samps += BL;
    }
}


void Recplay::real_run() {
    inp_samps = inp->run(current_block);
    gain_samps = gain->run(current_block);
    for (int i = 0; i < chans; i++) {
        run_channel(i);
        inp_samps += inp_stride;
        gain_samps += gain_stride;
    }
}


void Recplay::record(bool record) {
    if (lender_ptr) {
        arco_warn("Recplay: can't record into lender's buffer\n");
        return;
    }
    if (!buffers) {
        my_buffers.init(10);  // allocate space for some buffer pointers
        buffers = &my_buffers;
    }
    if (recording == record) {
        return;  // already recording/not recording
    }
    if (record) {
        rec_index = 0;
        recording = true;
        sample_count = 0;
    } else {
        recording = false;
        printf("Recplay::record stop, rec_index %ld end_count %ld\n",
               rec_index, sample_count);
    }
}


void Recplay::start(double start_time_)
{
    start_time = start_time_;
    if (!buffers) {
        arco_warn("Recplay: Nothing recorded to play");
        goto no_play;
    }
    play_index = (long) (start_time * AR);
    if (play_index < 0) {
        play_index = 0;
    } else if (play_index > END_COUNT) {
        play_index = END_COUNT;
    }
    play_phase = play_index;
    prev_sample = 0; // just to be safe, initialize for interpolation

    fade_count = long(fade * AR * BL_RECIP);
    fade_len = fade_count * BL;
    if (fade_len <= 0) {
        arco_warn("Recplay: fade_len too short, not starting.");
        goto no_play;
    }
    if (!RECORDING) {
        // round up to integral number of blocks
        long needed_blocks = long(fade_count * speed) + 1;
        if (needed_blocks <= 0) needed_blocks = 1;
        long recorded_blocks = (END_COUNT - play_index) / BL;
        if (needed_blocks * 2 > recorded_blocks) {
            // If fade is long and recording is short, there's no time for
            // fade-in/fade-out, and the anticipatory fade-out will set the
            // gain to 1 and cause a glitch. Therefore, split the recording
            // time to allow a fade-in followed by a fade-out.
            fade_count = long(recorded_blocks / (2 * speed));
            fade_len = fade_count * BL;
            if (fade_len <= 0) {
                arco_warn("Recplay: can't play very short recording");
                goto no_play;
            } else {
                arco_warn(
                        "Recplay: fade-in time cut to half of recording len.");
            }
        }
    }

    fade_incr = 100.0 / fade_len;
    fade_phase = 2.0; // phase goes from 2 to 102

    fading = true;
    stopping = false;
    playing = true;
    return;
no_play:
    send_action_id(action_id);
}


void Recplay::stop()
{
    // note: fading in and out are done on block boundaries to
    // simplify processing
    if (playing && !stopping) {
        fading = true;
        stopping = true;
        fade_count = long(fade * AR * BL_RECIP);
        fade_len = fade_count * BL;

        if (!RECORDING) {
            long needed_blocks = long(fade_count * speed + 1);
            // in case speed is zero or negative, force something reasonable
            if (needed_blocks <= 0) needed_blocks = 1;
            // do we have time to fade out?
            long blocks_left = ((END_COUNT - play_index) / BL) - 1;
            if (needed_blocks > blocks_left) {
                fade_count = long(blocks_left / speed);
                fade_len = fade_count * BL;
            }
        }
        // in case fade is zero or negative, force something reasonable
        if (fade_count <= 0) {
            fade_count = 1;
            fade_len = BL;
        }
        // on the other hand, if we're recording, we have more
        // time to fade, but since recording may stop, and we may
        // have speed > 1, we can still run out of samples to play.
        // This may cause a glitch and a sudden stop.

        fade_incr = -(fade_phase - 2.0) / fade_len;
        // phase will go from current value (normally 102.0) to 2.0
    }
}


void Recplay::borrow(int lender_id)
{
    // make sure we are borrowing buffers from a lender of
    if (buffers) {
        arco_warn("Irecplay::borrow called after buffer allocation");
        return;
    }

    UGEN_FROM_ID(Recplay, lender, lender_id, "arco_recplay_borrow lender");

    if (borrowing) { // return buffers to lender
        lender_ptr->return_buffers();
        borrowing = false;
        buffers = NULL;
    }

    if (!lender) return;
    buffers = &(lender->my_buffers);
    lender->ref();
    lender_ptr = lender;
    borrowing = true;
}


/* O2SM INTERFACE: /arco/recplay/new int32 id, int32 chans, 
              int32 inp_id, int32 gain_id, float fade, bool loop;
 */
void arco_recplay_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chans = argv[1]->i;
    int32_t inp_id = argv[2]->i;
    int32_t gain_id = argv[3]->i;
    float fade = argv[4]->f;
    bool loop = argv[5]->B;
    // end unpack message

    ANY_UGEN_FROM_ID(inp, inp_id, "arco_recplay_new inp");
    ANY_UGEN_FROM_ID(gain, gain_id, "arco_recplay_new gain");
    new Recplay(id, chans, inp, gain, fade, loop);
}


/* O2SM INTERFACE: /arco/recplay/repl_inp int32 id, int32 inp_id;
 */
static void arco_recplay_repl_inp(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t inp_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Recplay, recplay, id, "arco_recplay_repl_inp");
    ANY_UGEN_FROM_ID(inp, inp_id, "arco_recplay_repl_inp");
    recplay->repl_inp(inp);
}


/* O2SM INTERFACE: /arco/recplay/repl_gain int32 id, int32 gain_id;
 */
static void arco_recplay_repl_gain(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t gain_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Recplay, recplay, id, "arco_recplay_repl_gain");
    ANY_UGEN_FROM_ID(gain, gain_id, "arco_recplay_repl_gain");
    recplay->repl_gain(gain);
}


/* O2SM INTERFACE: /arco/recplay/set_gain int32 id, int32 chan, float gain;
 */
static void arco_recplay_set_gain(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chan = argv[1]->i;
    float gain = argv[2]->f;
    // end unpack message

    UGEN_FROM_ID(Recplay, recplay, id, "arco_recplay_repl_gain");
    recplay->set_gain(chan, gain);
}


/* O2SM INTERFACE: /arco/recplay/speed int32 id, float speed;
 */
static void arco_recplay_speed(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    float speed = argv[1]->f;
    // end unpack message

    UGEN_FROM_ID(Recplay, recplay, id, "arco_recplay_speed");
    recplay->set_speed(speed);
}


/* O2SM INTERFACE: /arco/recplay/rec int32 id, bool record;
 */
static void arco_recplay_rec(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    bool record = argv[1]->B;
    // end unpack message

    UGEN_FROM_ID(Recplay, recplay, id, "arco_recplay_rec");
    recplay->record(record);
}


/* O2SM INTERFACE: /arco/recplay/start int32 id, float start_time;
 */
static void arco_recplay_start(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    double start_time = argv[1]->d;
    // end unpack message

    UGEN_FROM_ID(Recplay, recplay, id, "arco_recplay_start");
    recplay->start(start_time);
}


/* O2SM INTERFACE: /arco/recplay/act int32 id, int32 action_id;
 *    set the action_id
 */
void arco_recplay_act(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t action_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Recplay, recplay, id, "arco_recplay_act");
    recplay->action_id = action_id;
    printf("arco_recplay_act: set ugen %d action_id %d\n", id, action_id);
}


/* O2SM INTERFACE: /arco/recplay/stop int32 id;
 */
static void arco_replay_stop(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    // end unpack message

    UGEN_FROM_ID(Recplay, recplay, id, "arco_recplay_stop");
    recplay->stop();
}


/* O2SM INTERFACE: /arco/recplay/borrow int32 id, int32 lender_id;
 */
static void arco_replay_borrow(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t lender_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Recplay, recplay, id, "arco_recplay_borrow id");
    recplay->borrow(lender_id);
}


static void recplay_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/recplay/new", "iiiifB", arco_recplay_new, NULL, true, true);
    o2sm_method_new("/arco/recplay/repl_inp", "ii", arco_recplay_repl_inp, NULL, true, true);
    o2sm_method_new("/arco/recplay/repl_gain", "ii", arco_recplay_repl_gain, NULL, true, true);
    o2sm_method_new("/arco/recplay/set_gain", "iif", arco_recplay_set_gain, NULL, true, true);
    o2sm_method_new("/arco/recplay/speed", "if", arco_recplay_speed, NULL, true, true);
    o2sm_method_new("/arco/recplay/rec", "iB", arco_recplay_rec, NULL, true, true);
    o2sm_method_new("/arco/recplay/start", "id", arco_recplay_start, NULL, true, true);
    o2sm_method_new("/arco/recplay/act", "ii", arco_recplay_act, NULL, true, true);
    o2sm_method_new("/arco/recplay/stop", "i", arco_replay_stop, NULL, true, true);
    o2sm_method_new("/arco/recplay/borrow", "ii", arco_replay_borrow, NULL, true, true);
    // END INTERFACE INITIALIZATION
}

Initializer recplay_init_obj(recplay_init);
