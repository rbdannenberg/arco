/* pwl -- unit generator for arco
 *
 * Roger B. Dannenberg
 * Jan 2022
 */

#include <climits>

extern const char *Pwl_name;

class Pwl : public Ugen {
public:
    float current;   // value of the next output sample
    int seg_togo;    // remaining samples in current segment
    float seg_incr;  // increment from one sample to the next in current segment
    float final_value; // target value that will be first sample of next segment
    int next_point_index;  // index into point of the next segment
    int action_id;
    Vec<float> points;  // envelope breakpoints (seg_len, final_value)

    Pwl(int id) : Ugen(id, 'a', 1) {
        current = 0.0f;        // real_run() will compute the first envelope
        seg_togo = INT_MAX;    //     after start(); initial output is 0
        seg_incr = 0.0;
        final_value = 0.0f;    // if no envelope is loaded, output will
        next_point_index = 0;  //     become constant zero.
        action_id = 0;
        points.init(0);        // initially empty, so size = 0
    }
    
    const char *classname() { return Pwl_name; }

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
                    if (current == 0 && (flags & CAN_TERMINATE)) {
                        terminate();
                    }
                } else {
                    seg_togo = (int) points[next_point_index++];
                    final_value = points[next_point_index++];
                    seg_incr = (final_value - current) / seg_togo;
                }
            }
            // invariant 0 < n <= BL and n <= seg_togo
            for (int i = 0; i < n; i++) {
                *out_samps++ = current;
                current += seg_incr;
            }
            togo -= n;
            seg_togo -= n;
        } while (togo > 0);
    }
    
    void start() {
        next_point_index = 0;
        seg_togo = 0;
        final_value = current;  // continue from current, whatever it is
    }

    void stop() {
        next_point_index = 0;
        seg_togo = INT_MAX;
        seg_incr = 0.0f;
    }

    void decay(float d) {
        seg_togo = (int) d;
        seg_incr = -current / seg_togo;
        next_point_index = points.size();  // end of envelope
        final_value = 0.0f;
    }

    void set(float y) {
        current = y;
    }
    
};

