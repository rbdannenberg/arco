/* zerob.h -- unit generator that passes audio through
 *
 * Roger B. Dannenberg
 * Jan 2022
 */

extern const char *zerob_name;

class Zerob : public Ugen {
public:
    Zerob(int id) : Ugen(id, 'a', 1) {
        current_block = MAX_BLOCK_COUNT;
        *out_samps = 0.0f; }

    const char *classname() { return zerob_name; }

    void real_run() { ; }
};
