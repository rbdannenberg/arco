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
    Vec<float> points;     // envelope breakpoints (seg_len, final_value)

    Pweb(int id) : Ugen(id, 'b', 1) {
        bias = 0.01;
        current = bias;   // real_run() will compute the first envelope
        seg_togo = 0;     //     segment as soon as it is called because 
                          //     seg_togo == 0
        final_value = bias;      // if no envelope is loaded, output will
        next_point_index = 0;    //     become constant zero.
        action_id = 0; }

    ~Pweb() { if (action_id) send_action_id(action_id); }

    const char *classname() { return Pweb_name; }

    void real_run() {
        if (seg_togo == 0) { // set up next segment
            current = final_value;  // make output value exact
            if (next_point_index >= points.size()) {
                seg_togo = INT_MAX;
                seg_factor = 1.0f;
                if (current == bias && action_id) {
                    send_action_id(action_id);
                }
            } else {
                seg_togo = (int) points[next_point_index++];
                final_value = points[next_point_index++] + bias;
                seg_factor = exp(log(final_value / current) / seg_togo);
                // printf("Pweb: togo %d final %g\n", seg_togo, final_value);
            }
        }
        *out_samps++ = current - bias;
        current *= seg_factor;
        seg_togo--;
    }
    
    void start() {
        next_point_index = 0;
        seg_togo = 0;
        final_value = current;  // continue from current, whatever it is
    }

    void decay(float d) {
        seg_togo = (int) d;
        final_value = bias;
        seg_factor = exp(log(final_value / current) / seg_togo);
        next_point_index = points.size();  // end of envelope
    }
    
    void point(float f) { points.push_back(f); }
};

