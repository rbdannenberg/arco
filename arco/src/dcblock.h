/* dcblock.h -- DC blocker, a first-order high-pass filter
 *
 * Roger B. Dannenberg
 * Oct 2023
 *
 * based on https://ccrma.stanford.edu/~jos/fp/DC_Blocker.html
 */

class Dcblock {
  public:
    Sample yprev;
    Sample xprev;

    Dcblock() { init(); }

    void init() { yprev = 0; xprev = 0; }

    Sample filter(Sample x) {
        Sample y = x - xprev + 0.995 * yprev;
        xprev = x;
        yprev = y;
        return y;
    }
};
