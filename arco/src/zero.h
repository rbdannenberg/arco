/* zero.h -- unit generator that passes audio through
 *
 * Roger B. Dannenberg
 * Jan 2022
 */

extern const char *Zero_name;

class Zero : public Ugen {
public:
    Zero(int id) : Ugen(id, 'a', 1) {
        current_block = MAX_BLOCK_COUNT;
        block_zero(out_samps);
    }

    const char *classname() { return Zero_name; }

    void real_run() { ; }
};
