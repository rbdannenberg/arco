/* random-number generation
 * eub  7/97
 * modified by rbd 2023
 */

#ifndef UGRAND_H
#define UGRAND_H


const unsigned lc_A = 196314165, lc_B = 907633515;	// for 32-bit!
const float lc_scale = (float)1.0/0xffffffff;

inline void silent_clamp(int &x, int lo, int hi)
// enforce x in [lo, hi].
{
    if (x < lo) {
        x = lo;
    } else if (x > hi) {
        x = hi;
    }
}


inline float interpolate(float x, float x1, float y1, float x2, float y2)
// interpolate along (x1, y1) to (x2, y2) at x
{
    return y1 + (y2 - y1) * (x - x1) / (x2 - x1);
}


inline unsigned lincongr_int()
{
    static unsigned state = 0;

    state *= lc_A;
    state += lc_B;
    return  state;
}


inline float unifrand()
{
    return  lc_scale*lincongr_int();
}


inline float unifrand_range(float x1, float x2)
{
    return interpolate(unifrand(), 0, x1, 1, x2);
}


inline unsigned lincongr2_int(unsigned &state)
{
    state *= lc_A;
    state += lc_B;
    return  state;
}

inline float unifrand2(unsigned &state)
{
    return  lc_scale*lincongr2_int(state);
}


class Pink {
  public:
    static const int Maxdice = 8;
    unsigned counter;
    int dice;
    int shift;
    float scale;	        // output is scale*sum(darr[]).
    unsigned darr[Maxdice];     // top _shift_ bits are zeroed.

    Pink(int dice_) {
        counter = 0;
        dice = dice_;

        int t = dice - 1;
        for (shift = 0; t; ++shift) {
            t >>= 1;
        }

        scale = (1 << shift) / (float) (dice * 0xffffffff * 2.0);

        for (int n = 0; n < dice; n++) {
            darr[n] = lincongr_int() >> shift;
        }
    }

    float pink() {
        int n;
        unsigned sum = 0,
	old = counter++,
	changed = old ^ counter;

        for (n =0; n < dice; n++) {
            sum += darr[n];
            if (1 & changed) {
                darr[n] = lincongr_int() >> shift;
            }
            changed >>= 1;
        }
        return sum * scale - 1.0;
    }
};

#endif
