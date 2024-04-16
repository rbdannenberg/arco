/* onset.h -- onset detection unit generator
 
 defitions for frame and buffer can be found at https://mural.maynoothuniversity.ie/4204/1/JT_Real-Time_Detection.pdf
 */

#include "detectionfunctions.h"
#include "onsetdetection.h"

extern const char *Onset_name;


class Onset : public Ugen {
  public:
    const char *address; // where to send messages

    Ugen_ptr input;
    int input_stride;
    Sample_ptr input_samps;
    
    int samps_stored;
    int frame_size;
    int buffer_size;
    double *frame;
    LPSpectralDifferenceODF* odf;
    RTOnsetDetection* detector;
    
    Onset(int id, Ugen_ptr input, const char *address_) : Ugen(id, 0, 0) {
        // round up to multiple of BL:
        address = o2_heapify(address_);
        init_input(input);
        
        samps_stored = 0;
        frame_size = 2048;
        buffer_size = 512;
        frame = O2_MALLOCNT(frame_size, double);
        odf = new LPSpectralDifferenceODF(); //Vec
        odf->init();
        detector = new RTOnsetDetection();
    }


    ~Onset() {
        O2_FREE((char *) address);
        O2_FREE(frame);
        //NEED TO CHANGE THIS
//        delete odf;
//        delete detector;
    }


    void init_input(Ugen_ptr ugen) { init_param(ugen, input, input_stride); }

    const char *classname() { return Onset_name; }
    
    void real_run() {
        input_samps = input->run(current_block);
        
        if (samps_stored == frame_size){ //if we have full frame
            for (int i = 0; i < frame_size - buffer_size; i++){ // i < buffer size
                frame[i] = frame[i + buffer_size];
            }
            samps_stored = 0;
        }
        
        //currently only uses one channel
        for (int i = 0; i < BL; i++){ //copy one block to frame buffer
            frame[i + samps_stored] = *(&input_samps[i]);
        }
        samps_stored += BL;
        
        int n = 10;
        
        if (samps_stored != frame_size) return; //compute odf if we have full frame
        
        float odf_res = odf->process_frame(frame_size, frame);
        
        if (detector->is_onset(odf_res)){
            o2sm_send_start();
            o2sm_add_int32(id);
            o2sm_add_float(odf_res);
            o2sm_send_finish(0, address, false);
        }
    }
};
