/* pwlb -- unit generator for arco
 *
 * Roger B. Dannenberg
 * Jan 2022
 */

#include <climits>

extern const char *Pwlb_name;

class Pwlb : public Ugen {
public:
    float current;   // value of the next output sample
    int seg_togo;    // remaining samples in current segment
    float seg_incr;  // increment from one sample to the next in current segment
    float final_value; // target value that will be first sample of next segment
    int next_point_index;  // index into point of the next segment
    int action_id;         // send this when envelope is done
    Vec<float> points;     // envelope breakpoints (seg_len, final_value)

    Pwlb(int id) : Ugen(id, 'b', 1) {
        current = 0.0f;        // real_run() will compute the first envelope
        seg_togo = INT_MAX;    //     after start(); initial output is 0
        seg_incr = 0.0;
        final_value = 0.0f;    // if no envelope is loaded, output will
        next_point_index = 0;  //     become constant zero.
        action_id = 0;
        points.init(0);        // initially empty, so size = 0
    }

    const char *classname() { return Pwlb_name; }


    void print_details(int indent) {
        arco_print("cur %f action %d", current, action_id);
    }

   
    void real_run() {
        // using while allows durations of zero so that we can have an
        // initial non-zero value by setting initial duration to 0:
        while (seg_togo == 0) { // set up next segment
            current = final_value;  // make output value exact
            if (next_point_index >= points.size()) {
                seg_incr = 0.0f;
                // if we can terminate, send_action_id when termination
                // is complete (done by terminate(ACTION_TERM)):
                if (current == 0 && (flags & CAN_TERMINATE)) {
                    // keep polling terminate until it happens:
                    if (terminate(ACTION_EVENT | ACTION_END)) {
                        seg_togo = INT_MAX;  // then this stops polling until start()
                    }
                } else {  // otherwise, just send_action_id now:
                    int status = ACTION_EVENT;
                    if (final_value == 0) {
                        status |= ACTION_END;
                    }
                    send_action_id(status);
                }
                break;
            } else {
                seg_togo = (int) points[next_point_index++];
                final_value = points[next_point_index++];
                seg_incr = (final_value - current) / seg_togo;
            }
        }
        *out_samps = current;
        current += seg_incr;
        seg_togo--;
    }
    

    void start() {
        next_point_index = 0;
        seg_togo = 0;
        final_value = current;  // continue from current, whatever it is
    }


    void stop() {
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
    

    void point(float f) { points.push_back(f); }
};

