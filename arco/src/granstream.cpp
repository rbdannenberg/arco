/* Igranstr instrument, granular synthesis on streaming input. */

// AVOIDING OVER/UNDERFLOW: We'll pull samples from buffer,
// so picture a graph with time horizontal and buffer
// position as vertical. Assume the buffer is infinitely
// long, so as the buffer fills, we can plot the boundary of
// valid samples as a stairstep starting at VL at the origin
// (because we read VL samples before anything else) and increasing
// by VL every VP seconds. Because the buffer is actually
// circular, the buffer is invalidated in a stairstep as well,
// with VL - bufferlen at the origin, and stepping by VL every
// VP seconds.
//     We want to guarantee that we only access samples from
// this valid region. Since the ratio is fixed, if we start
// and finish at valid points, then all intermediate points are
// valid.  To simplify conservatively, let's ignore
// the stairstep region and pretend the valid samples are in
// a diagonal band bounded above by t*AR (where t is time in secs)
// and below by t*AR + VL - bufferlen. Subtracting t*AR from
// these bounds we get from 0 to VL - bufferlen.
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
// which must lie between 0 and VL - bufferlen
// We interpolate by reading one sample ahead, so take this into
// account too, but basically, if we start within the buffer and 
// end within the buffer, all intermediate reads will be within
// the buffer.

#include "audioincl.h"
#include "uenv.h"
#include "umul.h"
#include "uadd.h"
#include "igranstr.h"

Igranstr::Igranstr()
{
    high = 1.0;
    low = 1.0;
    highdur = 0.05;
    lowdur = 0.05;
    delay = 0.0;
    density = 0.5;
    gate = 0.0;
    attack = 0.01;
    release = 0.01;
    polyphony = 0;  // see set_polyphony() below for real initial value
    stopped = true;
    warningblock = 0;

    buffer = NULL;
    bufferlen = 0;
    next_input = 0;
    stopped = true;
    // request_stop remains true until gate goes positive; request_stop
    // determines when we compute grains (not stopped)
    request_stop = true;
}


void Igranstr::showit()
{
    cout << "Igranstr " << aura << endl;
    cout << "    high " << high;
    cout << "    low " << low;
    cout << "    highdur " << highdur;
    cout << "    lowdur " << lowdur;
    cout << "    delay " << delay;
    cout << "    density " << density;
    cout << "    attack " << attack;
    cout << "    release " << release;
    cout << "    polyphony " << polyphony;
    cout << "    debuglevel " << debuglevel;
}


void Igranstr::init_io()
{
    /* (inputs, audio outputs, block outputs) */
    Instr::init_io(1, inarray, CHANNELS, aoutbuf, 0, NULL);
    gran_out = aoutbuf;
}


void Igranstr::set_in(Aura a, long chan)
{
    Instr *source_instr = (Instr *) ptr(a);
    if (!source_instr) aura_warn("Igranstr set_in(NULL) -- ignored.");
    link(0, source_instr, a_rate, (int) chan, 1);
    // note that link does bounds checking on source_instr and chan
    in_ptr = source_instr->outblock(a_rate, (int) chan);
}


void Igranstr::set_length(double length)
{
    if (buffer) {
        // set_length should not be called while processing audio
        // since it will destroy samples being read by grains.
        // To avoid reading random data, kill all the grains here
        reset_gens();
        UG_FREE_VEC(buffer, sample, bufferlen);
    }
    // this expression rounds to nearest sample
    bufferlen = (int) (0.5 + length * AR);
    // this expression rounds up to nearest multiple of block length
    // since VL is a power of 2, I use a fast masking operation:
    bufferlen = ((VL - 1) + bufferlen) & ~(VL - 1);
    cout << "Igranstr::set_length " << bufferlen << endl;
    buffer = UG_ALLOC_VEC(sample, bufferlen);
    UG_ZERO_VEC(buffer, sample, bufferlen);
}


void Igranstr::reset()
{
    if (buffer)
        UG_ZERO_VEC(buffer, sample, bufferlen);
}


void Igranstr::reset_gens()
{
    // set delay = 1 so that other parameters will be computed
    // on the next call to real_run():
    for (int i = 0; i < polyphony; i++) {
        gens[i].env.set_mode(env_lin);
        gens[i].state = GS_STOPPING;
        gens[i].delay = 1;
        // can't print to cout inside Igranstr::Igranstr(!)
        // show_grain("set_polyphony", i);
    }
}


void Igranstr::set_polyphony(int newpolyphony)
{
    if (newpolyphony <= 0 || newpolyphony == polyphony) return;
    if (gens) {
        UG_FREE_VEC(gens, Gran_gen, polyphony);
    }
    gens = UG_ALLOC_VEC(Gran_gen, newpolyphony);
    assert(gens);
    polyphony = newpolyphony;
    reset_gens();
}


void Igranstr::set_gate(double newgate)
{
    cout << "set_gate " << newgate << " gate " << gate << 
            " request_stop " << request_stop << endl;
    if ((gate <= 0.0) && (newgate > 0.0)) { // start the note:
        request_stop = false;
        stopped = false;
        if (gens) {
            for (int i = 0; i < polyphony; i++) {
                gens[i].state = GS_STOPPING;
                gens[i].delay = 1;
            }
        }
    } else if ((gate > 0.0) && (newgate <= 0.0)) { // stop it
        request_stop = true;
    }
    gate = newgate;
}


void Igranstr::show_grain(const char *label, int i)
{
    cout << "grain[" << i << "] (" << label << ") state ";
    cout << gens[i].state << " delay " << gens[i].delay;
    cout << " tostop " << gens[i].tostop << " release_blocks ";
    cout << gens[i].release_blocks << " dur_blocks " << gens[i].dur_blocks;
    cout << " ratio " << gens[i].ratio << " phase ";
    cout << gens[i].phase << endl;
}

void Igranstr::real_run(long newblock)
{
    int i;
    // first, read input audio into input buffer
    freshen_inputs(newblock);
    assert(next_input >= 0);
    assert(next_input + VL <= bufferlen);
    memcpy(buffer + next_input, in_ptr, sizeof(sample) * VL);
    // print_sample("inp", *in_ptr);

    next_input += VL;
    // wrap around inside buffer:
    if (next_input >= bufferlen) next_input = 0;

    memset(gran_out, 0, sizeof(gran_out[0]) * VL * CHANNELS);
    if (stopped) return;
    if (request_stop) stopped = true;  // detect if any grains are computing
    // stopped is reset to false if any grains are being computed
    // don't simply set stopped to true because we might have activity 
    // scheduled for the future and we don't want to stop just because
    // nothing is being computed now

    // print_sample("p", (float) polyphony);
    for (i = 0; i < polyphony; i++) {
        // schedule grains
        gens[i].run(*this, i);
    }
}


void Gran_gen::run(Igranstr &igranstr, int i)
{
    if (--delay == 0) {
        switch (state) {
        case GS_STOPPING: {
            if (igranstr.request_stop) return;
            // compute new parameters
            dur_blocks = (int)
                ((igranstr.lowdur + 
                  (igranstr.highdur - igranstr.lowdur) * 
                  lincongr()) * VR + 0.5);

            int attack_blocks = (int)(igranstr.attack * VR + 1);
            if (attack_blocks <= 0) attack_blocks = 1;
            attack = attack_blocks * VP;

            release_blocks = (int)(igranstr.release * VR + 1);
            if (release_blocks <= 0) release_blocks = 1;
            release = release_blocks * VP;

            if (attack_blocks + release_blocks > dur_blocks)
                dur_blocks = attack_blocks + release_blocks;

            ratio = igranstr.low + (igranstr.high - igranstr.low) * lincongr();
            // avgdur tries to factor out the attack/release
            // to get "effective avgdur"; assume effective duration
            // includes only half of attack and duration
            double avgdur = (igranstr.lowdur + igranstr.highdur - 
                             (igranstr.attack + igranstr.release)) * 0.5;
            double avgioi = avgdur * igranstr.polyphony / igranstr.density;
            // If all grains were the same duration (avgdur), we
            // could pick a delay from the end of one grain to the beginning
            // of the next grain to be between 0 and 2 * (ioi - avgdur)
            // (uniformly distributed) to obtain an average ioi of avgioi.
            // Things are more complicated when duration is not always 
            // avgdur, but we'll use the actual dur of the present grain:
            double ts = 2 * (avgioi - avgdur) * lincongr();
            if (ts < 0) ts = 0; // can't overlap grains
            int tostart = float_to_int(ts * VR);
            if (tostart < 0) tostart = 0;
            tostop = dur_blocks - release_blocks;
            delay = tostart;
            state = GS_PREDELAY;
            igranstr.stopped = false;
            if (tostart > 0) break;
        }
        case GS_PREDELAY: {
            // start grain computation 
            // print_sample("gens[i].tostart", gens[i].tostart);
            // print_sample("request_stop", (float) request_stop);
            if (igranstr.request_stop) return;
            // phase (here) is where we start in buffer relative to now
            phase = -igranstr.delay * AR * lincongr();
            // number of samples from input buffer is increment per
            // output sample * number of output samples
            double dur_in_samples = dur_blocks * VL;
            // To avoid underflow, check the following 
            // (see notes at top)
            // (another note: add 2 to allow interpolation 
            // and rounding):
            double final_phase = phase + 
                (dur_in_samples + 2) * (ratio - 1);
            if (final_phase < VL - igranstr.bufferlen) {
                // 1 sample fudge factor:
                double advance = (VL - igranstr.bufferlen) - final_phase - 1;
                    final_phase += advance;
                    phase += advance;
            } else if (final_phase > -1) {
                phase -= (final_phase + 1); // 1 sample fudge factor
                final_phase = -1;
            }
            // it may be that we can't satisfy both constraints, 
            // so check again. Also, make sure phase is initially 
            // in buffer. Only play grain if it is possible:
            if (phase > VL - igranstr.bufferlen && 
                final_phase > VL - igranstr.bufferlen && 
                phase < 0 && final_phase < 0) {
                // start the note
                env.set_dur(attack);
                env.set_goal(igranstr.gate);
                // print_sample("y1", gate);
                // note: phase is negative; map phase to buffer offset
                //igranstr.aura_printf("phase %g\n", phase);
                phase = igranstr.next_input - VL + phase;
                // in case ratio is 1, avoid interpolation
                phase = (long) phase; 
                if (phase < 0) phase += igranstr.bufferlen;
                //int relphase = (((long) phase) - igranstr.next_input + igranstr.bufferlen) % igranstr.bufferlen - igranstr.bufferlen;
                //igranstr.aura_printf("   relphase init %d\n", relphase);
                state = GS_STARTED;
                delay = tostop;
                assert(phase >= 0 && phase < igranstr.bufferlen);
            } else {
                delay = 1; // try again next block
                state = GS_PREDELAY;
                // only one message per 1000 blocks to avoid flood of errors
                if (igranstr.warningblock > igranstr.blocksdone) {
                    igranstr.aura_warn("igranstr grain too long\n");
                    igranstr.warningblock = igranstr.blocksdone + 1000;
                }
            }
            // otherwise note is too long, so skip it
            break;
        }
        case GS_STARTED: {
            state = GS_STOPPING;
            delay = release_blocks;
            // stop the note
            env.set_dur(release);
            env.set_goal(0.0);
            //int relphase = (((long) phase) - igranstr.next_input + igranstr.bufferlen) % igranstr.bufferlen - igranstr.bufferlen;
            //igranstr.aura_printf("   relphase offset %d\n", relphase);
            break;
        }
        }
        //cout << "# state[" << i << "] " << gens[i].state << " delay " <<
        //    gens[i].delay << " secs " << gens[i].delay / VR << endl;
    }
    if (state != GS_PREDELAY) { // GS_STARTED or GS_STOPPING
        assert(phase >= 0 && phase < igranstr.bufferlen);
        igranstr.stopped = false;
        sample ampl;
        sample block[VL]; // compute block of each grain here
        for (int j = 0; j < VL; j++) {
            int index = (int) phase;
            int index2 = index + 1;
            if (index2 >= igranstr.bufferlen) {
                index2 = 0;
            }
            assert(index >= 0 && index < igranstr.bufferlen);
            sample x1 = igranstr.buffer[index];
            assert(index2 >= 0 && index2 < igranstr.bufferlen);
            block[j] = x1 + (phase - index) * (igranstr.buffer[index2] - x1);
            phase = phase + ratio;
            // Do not merge this with the index2 >= igranstr.bufferlen test
            // above -- if ratio < 1, it may not be time to wrap
            // phase even if index2 has to wrap.
            if (phase >= igranstr.bufferlen) {
                phase -= igranstr.bufferlen;
            }
        }
        // int relphase = (((long) phase) - igranstr.next_input + igranstr.bufferlen) % igranstr.bufferlen - igranstr.bufferlen;
        //igranstr.aura_printf("   relphase run %ld\n", relphase);
        // print_sample("b", block[0]);
        env.run(ampl);
        // print_sample("a", ampl);
        mul.run(block, ampl, block);
        // distribute output among available channels
        sample *out_ptr = igranstr.gran_out + (VL * (i % CHANNELS));
        igranstr.add.run(block, out_ptr, out_ptr);
    }
}

#include "igranstr.aur"
