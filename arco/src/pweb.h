/* pweb -- unit generator for arco
 *
 * Roger B. Dannenberg
 * Jan 2022
 */

#include <climits>

extern const char *Pweb_name;

class Pweb : public Ugen {
public:
    float bias;      // The bias to add to breakpoints and subtract from output
    float current;     // value of the next output sample
    int seg_togo;      // remaining samples in current segment
    float seg_factor;  // current scale factor from one sample to the next
    float final_value; // target value that will be first sample of next segment
    int next_point_index;  // index into point of the next segment
    int action_id;         // send this when envelope is done
    bool linear_attack;    // first segment is linear
    bool linear_mode;      // first segment is linear and we're in the first segment
    Vec<float> points;     // envelope breakpoints (seg_len, final_value)

    Pweb(int id) : Ugen(id, 'b', 1) {
        bias = 0.01;
        current = bias;        // real_run() will compute the first envelope
        seg_togo = INT_MAX;    //     after start(); initial output is 0
        seg_factor = 1.0;
        final_value = bias;    // if no envelope is loaded, output will
        next_point_index = 0;  //     become constant zero.
        action_id = 0;
        points.init(0);   // initially empty, so size = 0
        linear_attack = false;
        linear_mode = false;   // set to linear_attack at beginning of envelope
    }

    const char *classname() { return Pweb_name; }

    void real_run() {
        if (seg_togo == 0) { // set up next segment
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
                    seg_factor = exp(log(final_value / current) / seg_togo);
                }
            }
        }
        current = (linear_mode ? current + seg_factor : current * seg_factor);
        *out_samps = current - bias;
        seg_togo--;
    }
    
    void start() {
        next_point_index = 0;
        linear_mode = linear_attack;
        seg_togo = 0;
        final_value = current;  // continue from current, whatever it is
        // printf("pweb start: final_value %g\n", final_value - bias);
    }


    void stop() {
        seg_togo = INT_MAX;
        seg_factor = !linear_mode;  // 1 if exp, 0 if linear
    }


    void linatk(bool linear) {
        linear_attack = linear;
    }


    void decay(int d) {
        seg_togo = d;
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

