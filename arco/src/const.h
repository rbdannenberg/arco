/* const.h -- unit generator that passes audio through
 *
 * Roger B. Dannenberg
 * Jan 2022
 */

extern const char *Const_name;

class Const : public Ugen {
public:
    Const(int id, int nchans, float value = 0.0f) : Ugen(id, 'c', nchans) {
        current_block = MAX_BLOCK_COUNT;
        // initial value is zero:
        for (int i = 0; i < nchans; i++) {
            output[i] = value;
        }
    };

    ~Const() { ; }

    const char *classname() { return Const_name; }

    void real_run() { ; }

    void print_tree(int indent, bool print, const char *parm);
};
