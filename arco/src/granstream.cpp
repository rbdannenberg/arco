/* granstream.cpp - granular synthesis on streaming input. */

// AVOIDING OVER/UNDERFLOW: We'll pull samples from buffer,
// so picture a graph with time horizontal and buffer
// position as vertical. Assume the buffer is infinitely
// long, so as the buffer fills, we can plot the boundary of
// valid samples as a stairstep starting at BL at the origin
// (because we read BL samples before anything else) and increasing
// by BL every VP seconds. Because the buffer is actually
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
// tells how far back in time we can grab audio data:
//     phase = lincongr() * delay * AR;
// The ending point is:
//     phase + dur * AR * ratio - (dur * AR),
// where dur is the time duration of the grain and ratio is
// the phase increment used to effect pitch shifting of grains.
// The -(dur * AR) term accounts for samples being added to the
// buffer. We can simplify to:
//     phase + dur * AR * (ratio - 1)
// which must lie between 0 and BL - bufferlen
// We interpolate by reading one sample ahead, so take this into
// account too, but basically, if we start within the buffer and 
// end within the buffer, all intermediate reads will be within
// the buffer.

#include "arcougen.h"
#include "fastrand.h"
#include "granstream.h"

const char *Granstream_name = "Granstream";

// declared here to break circular references among classes
//
bool Granstream_state::chan_a(Granstream *gs, Sample *out_samps) {
    // first, read input audio into input buffer
    block_copy(&samps[tail], gs->inp_samps);
    tail += BL;
    if (tail >= samps.size()) {
        tail = 0;
    }
    bool active = false;
    if (gs->enable && gs->gain > 0) {
        for (int i = 0; i < gs->polyphony; i++) {
            active |= gens[i].run(gs, this, out_samps, i);
        }
    }
    return active;
}


bool Gran_gen::run(Granstream *gs, Granstream_state *perchannel,
             Sample_ptr out_samps, int i) {
    int bufferlen = perchannel->samps.size();  //
    if (--delay == 0) {
        switch (state) {
          case GS_FALL: {  // fall has finished, now at end of envelope
            env_val = 0;
            env_inc = 0;
            if (!gs->enable || gs->gain == 0) {
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
            // pretend that all grains have the same avgdur duration. Since
            // avgdur does not contain half of attack and decay times, the
            // minimum ioi is:
            float minioi = avgdur + effective_attack_release;
            // the average ioi is based on density, polyphony, and
            // input channels:
            float avgioi = avgdur * gs->polyphony * gs->chans / gs->density;
            // we want to add a random time delay to minioi to achieve avgioi:
            float maxioi = avgioi + (avgioi - minioi);
            float ts = (maxioi - minioi) * unifrand();
            // Check the result: By substitution, letting
            //   ear = effective_attack_release, and
            //   unifrand() = 0.5 to represent the mean value,
            //   ts = (avgioi + (avgioi - avgdur - ear) - avgdur - ear) * 0.5
            //      = avgioi - avgdur - ear
            // So avgioi = ts + avgdur + ear. Thus using ts gives the right ioi.
            delay = (int) (ts * BR);
            // Make sure delay is plausible:
            if (delay < 0) delay = 0;
            state = GS_PREDELAY;
            if (delay > 0) {
                break;
            }  // otherwise, we skip the delay and immediately start ramping up:
          }
          case GS_PREDELAY: {  // predelay is finished, start grain computation
            if (!gs->enable || gs->gain == 0) {
                return false;
            }
            // phase (here) is where we start in buffer relative to now
            phase = -gs->dur * AR * unifrand();
            // number of samples from input buffer is increment per
            // output sample * number of output samples
            float dur_in_samples = dur_blocks * BL;
            // To avoid underflow, check the following (see notes at top)
            // (another note: add 2 to allow interpolation and rounding):
            float final_phase = phase + (dur_in_samples + 2) * ratio;
            if (final_phase < BL - bufferlen) {
                // 1 sample fudge factor:
                float advance = (BL - bufferlen) - final_phase - 1;
                final_phase += advance;
                phase += advance;
            } else if (final_phase > -1) {
                phase -= (final_phase + 1); // 1 sample fudge factor
                final_phase = -1;
            }
            // it may be that we can't satisfy both constraints, 
            // so check again. Also, make sure phase is initially 
            // in buffer. Only play grain if it is possible:
            if (phase > BL - bufferlen &&
                final_phase > BL - bufferlen &&
                phase < 0 && final_phase < 0) {
                // start the note
                env_val = 0;
                gain = gs->gain;  // copy gain from unit generator. Gain
                // for this grain will not be updated.
                env_inc = gain / (attack_blocks * BL);
                // note: phase is negative; map phase to buffer offset
                // in case ratio is 1, avoid interpolation
                phase = (int) (perchannel->tail - BL + phase);
                if (phase < 0) phase += bufferlen;
                state = GS_RISE;
                delay = attack_blocks;
                assert(phase >= 0 && phase < bufferlen &&
                       bufferlen == perchannel->samps.size());
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
        assert(perchannel->samps.bounds_check((int) phase));
        Sample ampl;
        // non-standard output: Instead of each channel (perchannel)
        // generating one channel of output, in this Ugen, grains
        // are spread across all output channels equally. Note
        // that out_samps is a parameter, so we are *not* updating
        // an instance variable, and neither does real_run().
        //
        // first, we select the output channel in round-robin fashion:
        out_samps += (i % gs->chans) * BL;
        for (int i = 0; i < BL; i++) {
            int index = (int) phase;
            Sample x = perchannel->samps[index];
            if (ratio != 1.0) {  // for tranposition, phase is fractional
                int index2 = index + 1;  // so do linear interpolation
                if (index2 >= bufferlen) {
                    index2 = 0;
                }
                Sample x2 = perchannel->samps[index2];
                x += (phase - index) * (x2 - x);
            }
            env_val += env_inc;
            *out_samps++ += x * env_val;
            phase += ratio;
            if (phase >= bufferlen) {
                phase -= bufferlen;
            }
        }
        return true;  // this grain is active
    }
    return false;  // this grain is not active
}


/* O2SM INTERFACE: /arco/granstream/new int32 id, int32 chans,
            int32 inp, int32 polyphony, float dur, bool enable;
 */
void arco_granstream_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chans = argv[1]->i;
    int32_t inp = argv[2]->i;
    int32_t polyphony = argv[3]->i;
    float dur = argv[4]->f;
    bool enable = argv[5]->B;
    // end unpack message

    ANY_UGEN_FROM_ID(inp_ugen, inp, "arco_granstream_new");

    new Granstream(id, chans, inp_ugen, polyphony, dur, enable);
}


/* O2SM INTERFACE: /arco/granstream/repl_inp int32 id, int32 inp_id;
 */
static void arco_granstream_repl_inp(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t inp_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Granstream, granstream, id, "arco_granstream_repl_inp");
    ANY_UGEN_FROM_ID(inp, inp_id, "arco_granstream_repl_inp");
    granstream->repl_inp(inp);
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
    printf("granstream ratios set %g %g\n", low, high);
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
    o2sm_method_new("/arco/granstream/repl_inp", "ii",
                     arco_granstream_repl_inp, NULL, true, true);
    o2sm_method_new("/arco/granstream/gain", "if", arco_granstream_gain, NULL,
                     true, true);
    o2sm_method_new("/arco/granstream/dur", "if", arco_granstream_dur, NULL,
                     true, true);
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
