/* const.h -- unit generator with set-able (piece-wise constant) output
 *
 * Roger B. Dannenberg
 * Jan 2022
 */

extern const char *Const_name;

class Const : public Ugen {
public:
    Const(int id, int nchans, float value = 0.0f) : Ugen(id, 'c', nchans) {
        current_block = MAX_BLOCK_COUNT;
        for (int i = 0; i < nchans; i++) {
            output[i] = value;
        }
    };


    ~Const() { ; }


    const char *classname() { return Const_name; }


    void real_run() { ; }


    void set_value(int chan, Sample value, const char *from) {
        if (!output.bounds_check(chan)) {
            arco_warn("Const::set_value id %d chan %d but actual chans"
                      " is %d", id, chan, chans);
            if (from) {
                arco_warn("Const::set_value was called from %s", from);
            }
            return;
        }
        output[chan] = value;
    }

};
