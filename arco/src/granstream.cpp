/* granstream.cpp - granular synthesis on streaming input. */

// AVOIDING OVER/UNDERFLOW: We'll pull samples from buffer,
// so picture a graph with time horizontal and buffer
// position as vertical. Assume the buffer is infinitely
// long, so as the buffer fills, we can plot the boundary of
// valid samples as a stairstep starting at BL at the origin
// (because we read BL samples before anything else) and increasing
// by BL every BP seconds. Because the buffer is actually
// circular, the buffer is invalidated in a stairstep as well,
// with BL - bufferlen at the origin, and stepping by BL every
// VP seconds.
//     We want to guarantee that we only access samples from
// this valid region. Since the ratio is fixed, if we start
// and finish at valid points, then all intermediate points are
// valid.  To simplify conservatively, let's ignore
// the stairstep region and pretend the valid samples are in
// a diagonal band bounded above by t*AR (where t is time in secs)
// and below by t*AR + BL - bufferlen. Subtracting t*AR from
// these bounds we get from 0 to BL - bufferlen.
// The starting point in the buffer is calculated as
// a random number between 0 and delay * AR, where delay 
// tells how far back in time we can grab audio data, and phase
// is how far back in time (in samples) we actually read. Note
// that we are changing our perspective from the vertical axis
// of the plot (buffer position) to "phase", which is how far back
// we reach from the tail of the buffer ("now"):
//     phase = random() * delay * AR;
// The ending point is:
//     phase + dur * AR * ratio - (dur * AR),
// where dur is the time duration of the grain and ratio is
// the phase increment used to effect pitch shifting of grains.
// The (dur * AR) term accounts for samples being added to the
// buffer. We can simplify to:
//     phase + dur * AR * (1 - ratio)
// which must lie between 0 and bufferlen - BL
// We interpolate by reading one sample ahead, so take this into
// account too, but basically, if we start within the buffer and 
// end within the buffer, all intermediate reads will be within
// the buffer.


#include "arcougen.h"
#include "fastrand.h"
#include "ringbuf.h"
#include "dcblock.h"
#include "granstream.h"

// Define SEEFB to "see feedback" on first output channel, from which
// grains are suppressed (for debugging only)
// #define SEEFB 1

const char *Granstream_name = "Granstream";

// declared here to break circular references among classes
//
bool Granstream_state::chan_a(Granstream *gs, Sample *out_samps, int chan) {
    // first, read input audio into input buffer
    input_buf.enqueue_block(gs->input_samps);
    // if feedback enabled, add feedback from delay buffer:

    Ringbuf &feedback_buf = gs->feedback_buf;

    if (this == &(gs->states[0]) && gs->feedback_delay > 0) {
        if (gs->feedback > 0.00003 || gs->feedback_out_smoothed > 0.00003) {
            // the feedback loop gain is roughly like feedback * density
            // since the final output is the sum of all the grains. If
            // density is high, we want to reduce feedbak when density is
            // high. However dividing by high density gives very low feedback
            // so we divide by sqrt(density). On the other hand, if density
            // is < 1, dividing by density will produce very high feedback
            // amplitudes that will clip, hence the use of fmax(1.0, ...):
            Sample target = gs->feedback / fmax(1.0, sqrt(gs->density));
            for (int i = 0; i < BL; i++) {
                gs->feedback_out_smoothed = target +
                        gs->feedbackfactor *
                        (gs->feedback_out_smoothed - target);
                Sample fb = feedback_buf.dequeue() * gs->feedback_out_smoothed;
                fb = gs->dcblock.filter(fb);
                input_buf.add_to_nth(fb, BL - i);
#ifdef SEEFB
                out_samps[i] = fb * 0.1;  // force chan 1 to be just feedback
                // scaled by 0.1 because we want to see the waveform even when
                // it is out of [-1, +1] range
#endif
            }
            D printf("fbgain %g\n", gs->feedback_out_smoothed);
        } else {  // need to remove samples from input_buf in any case
            input_buf.toss(BL);
        }
    }
    bool active = false;
    // active_polyphony is how many grains are actually running
    if (gs->polyphony > actual_polyphony) {
        actual_polyphony = gs->polyphony;
    }
    // actual_active_polyphony is how many grains to we need to run
    // next time (maybe some grains above polyphony finish running):
    int actual_active_polyphony = 0;
    if (gs->enable && gs->gain > 0) {
        for (int i = 0; i < actual_polyphony; i++) {
            if (gens[i].run(gs, this, out_samps, chan, i)) {
                active = true;
                actual_active_polyphony = i + 1;
            }
        }
        actual_polyphony = actual_active_polyphony;
    }
    return active;
}


bool Gran_gen::run(Granstream *gs, Granstream_state *perchannel,
                   Sample_ptr out_samps, int chan, int index) {
    int bufferlen = perchannel->input_buf.get_fifo_len();
    if (--delay == 0) {
        switch (state) {
          case GS_FALL: {  // fall has finished, now at end of envelope
            env_val = 0;
            env_inc = 0;
            state = GS_PREDELAY;  // this state is not computing a grain, so
            // if we fall through to the next test, it will test false and
            // we will return false. Alternatively, if requested we stop by
            // returning false and not computing a delay-until-next-grain.
            // When we are called again, delay will become negative, so
            // both (--delay == 0) and (gs->state != GS_PREDELAY) will fail,
            // so again we will return false (i.e. not computing a grain).
            // The only way to get back to computing a grain is to call
            // set_polyphony() which resets delays to 1 (positive) and state
            // to GS_FALL so this case is re-entered, the test immediately
            // below fails (because gs->stop_request is cleared), and we
            // compute parameters for the next grain.
            //     The final test is that when polyphony is reduced we only
            // start new grains at this generator index when it is less than
            // polyphony.
            if (gs->stop_request || gs->gain == 0 || index >= gs->polyphony ) {
                return false;  // do not start another grain; we're done
            }
            dur_blocks = (int) (unifrand_range(gs->lowdur, gs->highdur) *
                                BR + 0.5);
            attack_blocks = (int) (gs->attack * BR) + 1;
            if (attack_blocks < 0) {
                attack_blocks = 1;
            }
            release_blocks = (int) (gs->release * BR) + 1;
            if (release_blocks < 0) {
                release_blocks = 1;
            }
            if (attack_blocks + release_blocks > dur_blocks) {
                dur_blocks = attack_blocks + release_blocks;
            }
            ratio = unifrand_range(gs->low, gs->high);
            // avgdur tries to factor out the attack/release
            // to get "effective avgdur"; assume effective duration
            // includes only half of attack and duration:
            float effective_attack_release = (gs->attack + gs->release) * 0.5;
            float avgdur = (gs->lowdur + gs->highdur) * 0.5 -
                           effective_attack_release;
            // pretend that all grains have the same avgdur duration and are
            // separated by the same avgioi duration. Since avgdur does not
            // contain half of attack and decay times, the minimum ioi is:
            float minioi = avgdur + effective_attack_release;
            // the average ioi is based on density, polyphony, and
            // input channels. The density for a single grain stream is
            // gs->polyphony / avgioi. The factor of gs->polyphony accounts
            // for the fact that we have multiple grain streams. We do NOT
            // depend on gs->chans because grains are simply distributed among
            // output channels, but density is based on total grains, not
            // per-channel grain counts. Solve for avgioi:
            float avgioi = gs->density <= 0 ? MAX_BLOCK_COUNT * BP :
                                   gs->polyphony * avgdur / gs->density;
            // we want to add a random time delay to minioi to achieve avgioi:
            float maxioi = avgioi + (avgioi - minioi);
            // compute ts = "random ioi in range" - (avdur + ear) =
            //              "random ioi in range" - minioi:
            float ts = (maxioi - minioi) * unifrand();
            // So avgioi = ts + avgdur + ear. Thus using ts gives the right ioi.
            // avoid overflow:
            double delay_blocks = ts * BR;
            delay = delay_blocks > MAX_BLOCK_COUNT ? MAX_BLOCK_COUNT
                                                   : (int) delay_blocks;
            D printf("grain dens %g poly %d ts %g delay_blocks %d\n",
                   gs->density, gs->polyphony, ts, delay);
            // Make sure delay is plausible:
            if (delay < 0) delay = 0;
            if (delay > 0) {
                break;
            }  // otherwise, we skip the delay and immediately start ramping up:
          }
          case GS_PREDELAY: {  // predelay is finished, start grain computation
            if (!gs->enable || gs->gain == 0) {
                return false;
            }
            // phase (here) is where we start in buffer relative to now
            // (positive phase means earlier in time, measured in samples)
            // phase is selected in the range that starts and ends BL away
            // from the delay buffer boundaries:
            phase = (gs->dur * AR - 2 * BL) * unifrand() + BL;
            // number of samples from input buffer is increment per
            // output sample * number of output samples
            float dur_in_samples = dur_blocks * BL;
            // To avoid underflow, check the following (see notes at top)
            // (another note: add 2 to allow interpolation and rounding).
            // Note: if ratio > 1, the phase (how far back in time we go
            // into the input buffer) *decreases* because we're playing out
            // of the input buffer faster than we're recording into it,
            // the the proper term here really is (1 - ratio), not (ratio - 1):
            float final_phase = phase + (dur_in_samples + 2) * (1 - ratio);
            if (final_phase > bufferlen - BL) {  // too far back in time
                // 1 sample fudge factor:
                float advance = final_phase - (bufferlen - BL - 1);
                final_phase -= advance;  // now final_phase is bufferlen-BL-1
                phase -= advance;
            } else if (final_phase < (BL + 1)) {  // not far enough back in time
                phase += ((BL + 1) - final_phase); // 1 sample fudge factor
                final_phase = BL + 1;
            }
            // it may be that we can't satisfy both constraints, 
            // so check again. Also, make sure phase is initially 
            // in buffer. Only play grain if it is possible:
            if (phase < bufferlen - BL && final_phase < bufferlen - BL &&
                phase > BL && final_phase > BL) {
                // start the note
                env_val = 0;
                gain = gs->gain;  // copy gain from unit generator. Gain
                // for this grain will not be updated.
                env_inc = gain / (attack_blocks * BL);
                // note: phase is negative; map phase to buffer offset
                // in case ratio is 1, avoid interpolation
                state = GS_RISE;
                delay = attack_blocks;
                assert(phase > BL && phase < bufferlen &&
                       bufferlen == perchannel->input_buf.get_fifo_len());
                D printf("grain start: chan %d gen %ld phase %g dur %g "
                         "ratio %g\n",
                       (int) (perchannel - &(gs->states[0])),
                       (long) (this - &(perchannel->gens[0])), 
                       phase, dur_blocks * BP, ratio);
            } else {
                delay = 1; // try again next block
                state = GS_PREDELAY;
                // only one message per 1000 blocks to avoid flood of errors
                if (gs->warning_block > gs->current_block) {
                    printf("granstream grain too long\n");
                    gs->warning_block = gs->current_block + 1000;
                }
            }
            break;
          }
          case GS_RISE:  // attack has finished, start hold until fall
            delay = dur_blocks - attack_blocks - release_blocks;
            state = GS_HOLD;
            env_val = 1;
            env_inc = 0;
            if (delay > 0) {
                break;
            } // otherwise begin fall immediately
          case GS_HOLD:  // hold has finished, start fall
            delay = release_blocks;
            env_inc = -gain / (release_blocks * BL);
            state = GS_FALL;
            break;
        }
    }
    if (state != GS_PREDELAY) {  // envelope is non-zero
        assert(perchannel->input_buf.bounds_check((int) phase));
        Sample ampl;
        // non-standard output: Instead of each channel (perchannel)
        // generating one channel of output, in this Ugen, grains
        // are spread across all output channels equally. Note
        // that out_samps is a parameter, so we are *not* updating
        // an instance variable, and neither does real_run().
        //
        // first, we select the output channel in round-robin fashion:
        assert(ratio > 0);
#ifndef SEEFB
        out_samps += ((chan + index) % gs->chans) * BL;
#else
        assert(gs->chans > 1);  // SEEFB requires at least 2 channels
        out_samps += BL * ((chan + index) % (gs->chans - 1));
#endif
        for (int i = 0; i < BL; i++) {
            int index = (int) phase;
            Sample x = perchannel->input_buf.get_nth(index);
            if (ratio != 1.0) {  // for tranposition, phase is fractional
                int index2 = index + 1;  // so do linear interpolation
                Sample x2 = perchannel->input_buf.get_nth(index2);
                x += (phase - index) * (x2 - x);
            }
            env_val += env_inc;
            *out_samps++ += x * env_val;
            phase -= ratio;  // tricky - on average, the phase really
            // advances by (ratio - 1) each sample, but since the tail
            // got added to buf in a block and not while we're in this
            // loop, we have to advance by ratio. Also, phase is how far
            // *back* we access, so to advance in the buffer, we have to
            // decrease phase, hence -= instead of +=.
        }
        phase += BL;  // before we are called back, BL samples will be
        // inserted at the tail, so we need to bump phase by BL so that
        // it will reference the same location in time.
        return true;  // this grain is active
    }
    return false;  // this grain is not active
}


/* O2SM INTERFACE: /arco/granstream/new int32 id, int32 chans,
            int32 input_id, int32 polyphony, float dur, bool enable;
 */
void arco_granstream_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chans = argv[1]->i;
    int32_t input_id = argv[2]->i;
    int32_t polyphony = argv[3]->i;
    float dur = argv[4]->f;
    bool enable = argv[5]->B;
    // end unpack message

    ANY_UGEN_FROM_ID(input, input_id, "arco_granstream_new");

    new Granstream(id, chans, input, polyphony, dur, enable);
}


/* O2SM INTERFACE: /arco/granstream/repl_input int32 id, int32 input_id;
 */
static void arco_granstream_repl_input(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t input_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Granstream, granstream, id, "arco_granstream_repl_input");
    ANY_UGEN_FROM_ID(input, input_id, "arco_granstream_repl_input");
    granstream->repl_input(input);
}


/* O2SM INTERFACE: /arco/granstream/gain int32 id, float gain;
 */
void arco_granstream_gain(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    float gain = argv[1]->f;
    // end unpack message

    UGEN_FROM_ID(Granstream, granstream, id, "arco_granstream_gain");
    granstream->set_gain(gain);
}


/* O2SM INTERFACE: /arco/granstream/dur int32 id, float dur;
 */
void arco_granstream_dur(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    float dur = argv[1]->f;
    // end unpack message

    UGEN_FROM_ID(Granstream, granstream, id, "arco_granstream_dur");
    granstream->set_dur(dur);
}


/* O2SM INTERFACE: /arco/granstream/delay int32 id, float delay;
 */
void arco_granstream_delay(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    float delay = argv[1]->f;
    // end unpack message

    UGEN_FROM_ID(Granstream, granstream, id, "arco_granstream_delay");
    granstream->set_delay(delay);
}


/* O2SM INTERFACE: /arco/granstream/feedback int32 id, float feedback;
 */
void arco_granstream_feedback(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    float feedback = argv[1]->f;
    // end unpack message

    UGEN_FROM_ID(Granstream, granstream, id, "arco_granstream_feedback");
    granstream->set_feedback(feedback);
}


/* O2SM INTERFACE: /arco/granstream/polyphony int32 id, int32 polyphony;
 */
static void arco_granstream_polyphony(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t polyphony = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Granstream, granstream, id, "arco_granstream_polyphony");
    granstream->set_polyphony(polyphony);
}


/* O2SM INTERFACE: /arco/granstream/ratio int32 id, float low, float high;
 */
static void arco_granstream_ratio(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    float low = argv[1]->f;
    float high = argv[2]->f;
    // end unpack message

    UGEN_FROM_ID(Granstream, granstream, id, "arco_granstream_ratio");
    granstream->low = low;
    granstream->high = high;
    // printf("granstream ratios set %g %g\n", low, high);
}


/* O2SM INTERFACE: /arco/granstream/graindur int32 id,
                   float lowdur, float highdur;
 */
static void arco_granstream_graindur(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    float lowdur = argv[1]->f;
    float highdur = argv[2]->f;
    // end unpack message

    UGEN_FROM_ID(Granstream, granstream, id, "arco_granstream_graindur");
    granstream->lowdur = lowdur;
    granstream->highdur = highdur;
}


/* O2SM INTERFACE: /arco/granstream/density int32 id, float density;
 */
static void arco_granstream_density(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    float density = argv[1]->f;
    // end unpack message

    UGEN_FROM_ID(Granstream, granstream, id, "arco_granstream_density");
    granstream->density = density;
}


/* O2SM INTERFACE: /arco/granstream/env int32 id, float attack, float release;
 */
static void arco_granstream_env(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    float attack = argv[1]->f;
    float release = argv[2]->f;
    // end unpack message

    UGEN_FROM_ID(Granstream, granstream, id, "arco_granstream_env");
    granstream->attack = attack;
    granstream->release = release;
}


/* O2SM INTERFACE: /arco/granstream/enable int32 id, bool enable;
 */
static void arco_granstream_enable(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    bool enable = argv[1]->B;
    // end unpack message

    UGEN_FROM_ID(Granstream, granstream, id, "arco_granstream_enable");
    granstream->set_enable(enable);
}


static void granstream_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/granstream/new", "iiiifB", arco_granstream_new,
                    NULL, true, true);
    o2sm_method_new("/arco/granstream/repl_input", "ii",
                    arco_granstream_repl_input, NULL, true, true);
    o2sm_method_new("/arco/granstream/gain", "if", arco_granstream_gain, NULL,
                    true, true);
    o2sm_method_new("/arco/granstream/dur", "if", arco_granstream_dur, NULL,
                    true, true);
    o2sm_method_new("/arco/granstream/delay", "if", arco_granstream_delay,
                    NULL, true, true);
    o2sm_method_new("/arco/granstream/feedback", "if",
                    arco_granstream_feedback, NULL, true, true);
    o2sm_method_new("/arco/granstream/polyphony", "ii",
                    arco_granstream_polyphony, NULL, true, true);
    o2sm_method_new("/arco/granstream/ratio", "iff", arco_granstream_ratio,
                    NULL, true, true);
    o2sm_method_new("/arco/granstream/graindur", "iff",
                    arco_granstream_graindur, NULL, true, true);
    o2sm_method_new("/arco/granstream/density", "if", arco_granstream_density,
                    NULL, true, true);
    o2sm_method_new("/arco/granstream/env", "iff", arco_granstream_env, NULL,
                    true, true);
    o2sm_method_new("/arco/granstream/enable", "iB", arco_granstream_enable,
                    NULL, true, true);
    // END INTERFACE INITIALIZATION
}

Initializer granstream_init_obj(granstream_init);
