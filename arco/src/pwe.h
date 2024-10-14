/* pwe -- piece-wise exponential unit generator for arco
 *
 * Roger B. Dannenberg
 * June 2023
 */

// piece-wise exponential
//
// control points are d0 y0 d1 y1 ... dn-1 [yn-1] where yn-1 defaults to
// 0.0. HOWEVER, all yi points are shifted positively by bias, which defaults
// to 0.01 (about -40dB), exponential decays are computed, and then the
// bias is removed from the result. If yi is 0 the output will be zero, but
// the shape of the exponential decay will be what you would see in a decay
// to bias (0.01).
//    In addition, because it often sounds better, the first segment can be
// linear from 0 to y0 in time d0. This option is enabled by linatk (Boolean,
// default is True).
//    All breakpoints are rounded to the nearest sample time, but they may
// occur at any time with block computations.

#include <climits>

extern const char *Pwe_name;

class Pwe : public Ugen {
public:
    float bias;     // The bias to add to breakpoints and subtract from output
    float current;     // value of the next output sample + bias
    int seg_togo;      // remaining samples in current segment
    float seg_factor;  // current scale factor from one sample to the next
    float final_value; // target value that will be first sample of next segment
                       // target value includes bias
    int next_point_index;  // index into point of the next segment
    int action_id;      // send this when the envelope is done
    bool linear_attack; // first segment is linear
    bool linear_mode;   // first segment is linear and we're in first segment
    Vec<float> points;  // envelope breakpoints (seg_len, final_value)

    Pwe(int id) : Ugen(id, 'a', 1) {
        bias = 0.01;
        current = bias;        // real_run() will compute the first envelope
        seg_togo = INT_MAX;    //     after start(); initial output is 0
        seg_factor = 1.0;
        final_value = bias;    // if no envelope is loaded, output will
        next_point_index = 0;  //     become constant zero.
        action_id = 0;
        points.init(0);        // initially empty, so size = 0
        linear_attack = false;
        linear_mode = false;   // set to linear_attack at beginning of envelope
    }
    
    const char *classname() { return Pwe_name; }

    void real_run() {
        int togo = BL;  // how many samples left to compute before returning
        do {
            int n = seg_togo;
            // n = number of samples remaining in current segment
            if (n > togo) n = togo;
            // n = number of samples to compute in this segment and this
            //     iteration, or 0 if we need to move to next segment
            if (n == 0) { // set up next segment
                current = final_value;  // make output value exact
                if (next_point_index >= points.size()) {
                    stop();
                    send_action_id();
                    if (current == bias && (flags & CAN_TERMINATE)) {
                        terminate();
                    }
                } else {
                    linear_mode &= (next_point_index == 0);  // clear after 1st segment
                    seg_togo = (int) points[next_point_index++];
                    final_value = points[next_point_index++] + bias;
                    if (linear_mode) {
                        seg_factor = (final_value - current) / seg_togo;
                    } else {
                        // derivation: f^n = final_value / current
                        //       ln(f) * n = ln(final_value / current)
                        //          f      = exp(ln(final_value / current) / n)
                        seg_factor = exp(log(final_value / current) / seg_togo);
                    }
                }
            }
            if (linear_mode) {
                for (int i = 0; i < n; i++) {
                    current += seg_factor;
                    *out_samps++ = current - bias;
                }
            } else {
                // invariant 0 < n <= BL and n <= seg_togo
                for (int i = 0; i < n; i++) {
                    current *= seg_factor;
                    *out_samps++ = current - bias;
                }
            }
            togo -= n;
            seg_togo -= n;
        } while (togo > 0);
    }
    

    // start the envelope; continue from the current value
    void start() {
        next_point_index = 0;
        linear_mode = linear_attack;
        seg_togo = 0;
        final_value = current;  // continue from current, whatever it is
    }


    void stop() {
        seg_togo = INT_MAX;
        seg_factor = !linear_attack;
    }


    void linatk(bool linear) {
        linear_attack = linear;
    }


    // start decay to zero immediately for d samples. Skip remaining points.
    void decay(float d) {
        seg_togo = (int) d;
        final_value = bias;
        linear_mode = false;  // always exponential decay
        seg_factor = exp(log(final_value / current) / seg_togo);
        next_point_index = points.size();  // end of envelope
    }

    void set(float y) {
        current = y + bias;
    }
    
    void point(float f) { points.push_back(f); }

};

