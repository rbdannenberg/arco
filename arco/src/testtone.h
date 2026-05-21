/* testtone.h -- unit generator that makes a soft 1kHz sine tone
 *
 * Roger B. Dannenberg
 * Jan 2022
 */

extern const char *Testtone_name;

class Testtone : public Ugen {
public:
    double phase;
    Testtone(int id) : Ugen(id, 'a', 1) { phase = 0.0; }

    const char *classname() { return Testtone_name; }

#if ARCO_REF_DEBUG
    // for tracing tree of Ugens. Returns true with the ith child in *child
    // or false if i is too high.
    bool get_ref(int i, Ugen **child) {
        return false;  // Testtone has no Ugen children
    }
#endif

    void real_run() {
        Sample_ptr dst = &output[0];
        for (int i = 0; i < BL; i++) {
            *dst++ = sin(phase) * 0.1;
            phase += 1000.0 * PI2 / AR;
            if (phase > PI2) phase -= PI2;
        }
    }
};
