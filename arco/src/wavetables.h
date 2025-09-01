/* wavetables -- abstract superclass for unit generators with wavetables
 *
 * Roger B. Dannenberg
 * Oct 2024
 */

/* wavetables of length n represent a period of length n-2 where
 * w[0] repeats at w[n-2] and w[1] repeats at w[n-1] to simplify
 * interpolation.
 */
#include "ffts_compat.h"  // for ilog2()

typedef Vec<float> Wavetable;

const int sine_table_len = 1024;
extern float sine_table[sine_table_len + 1];

class Wavetables : public Ugen {
public:
    Vec<Wavetable> wavetables;
    Wavetables *lender;  // when non-null, wavetables are fetched from lender,
                         // allowing wavetables to be shared by many oscillators

    Wavetables(int id, char rate, int nchans) : Ugen(id, rate, nchans), wavetables(0) {
        lender = nullptr;
    }


    ~Wavetables() {
        for (int i = 0; i < wavetables.size(); i++) {
            wavetables[i].finish();
        }
        if (lender) {
            lender->unref();
        }
    }


    void borrow(Wavetables *wt) {
        lender = wt;
        lender->ref();
    }


    int num_tables() {
        return lender ? lender->wavetables.size() : wavetables.size();
    }


    Wavetable *get_table(int i) {
        Vec<Wavetable> &w = (lender ? lender->wavetables : wavetables);
        if (i < 0 || i >= w.size()) {  // convert invalid index to index 0
            if (w.size() > 0) {
                i = 0;
            } else {
                return nullptr;
            }
        }
        return &w[i];
    }


    void create_table_at(int i, int tlen) {
        // if i > size, extend wavetables and initialize to empty wavetables
        // round tlen up to the next power of 2 if needed
        if ((tlen & (tlen - 1)) != 0) {
            tlen = 1 << ilog2(tlen);
        }
        int n = wavetables.size();
        if (n <= i) {
            wavetables.set_size(i + 1, false);
            for (; n < i + 1; n++) {
                wavetables[i].init(0);
            }
        }
        wavetables[i].set_size(tlen + 2, false);
    }

    double schroeder_phase(int n, int slen) {
        // here, n is zero-based, so use (n + 1) * n rather than n * (n - 1):
        return M_PI * (n + 1) * n / slen;
    }

    // Uses Schroeder's formula for phase, intended to reduce crest factor
    // by avoiding all in-phase harmonics: φ(n) = π * n * (n - 1) / N
    void create_table(int i, int tlen, int slen, float *spec, bool hasphase) {
        create_table_at(i, tlen);
        // now wavetables[i] exists and is initialized but may be wrong size
        Wavetable &table = wavetables[i];
        tlen = table.size() - 2;  // length is rounded up to power of 2 + 2
        table.zero();
        int harm = 1;  // harmonic number
        for (int h = 0; h < slen - hasphase; h += 1 + hasphase) {
            float *data = &table[0];
            float amp = spec[h];
            double phase = (hasphase ? spec[h + 1] : schroeder_phase(h, slen));
            double phase_incr = (double) harm * sine_table_len / tlen;
            for (i = 0; i < tlen + 2; i++) {
                int iphase = phase;
                float phase_frac = phase - iphase;
                *data++ += (sine_table[iphase] * (1 - phase_frac) +
                            sine_table[iphase + 1] * phase_frac) * amp;
                phase += phase_incr;
                if (phase >= sine_table_len) {
                    phase -= sine_table_len;
                }
            }
            harm++;
        }
    }


    // set ith table of length tlen from an amplitude spectrum of length alen
    // tlen is the "period" of the table and 
    // the actual allocation size is tlen + 2
    void create_tas(int i, int tlen, int slen, float *ampspec) {
        create_table(i, tlen, slen, ampspec, false);
    }


    // set ith table of length tlen from a complex spectrum (amplitude and
    // phase) of length alen. Phase is in *radians*.
    void create_tcs(int i, int tlen, int slen, float *spec) {
        create_table(i, tlen, slen, spec, true);
    }


    // set ith table of length tlen from time domain data
    void create_ttd(int i, int tlen, float *samps) {
        create_table_at(i, tlen);
        // now wavetables[i] exists and is initialized but may be wrong size
        Wavetable &table = wavetables[i];
        float *data = &table[0];
        memcpy(data, samps, tlen * sizeof(float));
        table[tlen] = samps[0];
        table[tlen + 1] = samps[1];
    }
};

