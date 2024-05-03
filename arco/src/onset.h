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
    
    int frame_size;
    int buffer_size;
    int input_chans;
    int samps_stored;
    
    // one for each channel
    Vec<double*> frames;
    LPSpectralDifferenceODF** odfs;
    RTOnsetDetection** detectors;
    
    // for freeing
    static Vec<LPSpectralDifferenceODF*> old_odfs;
    static Vec<RTOnsetDetection*> old_detectors;
    
    Onset(int id, Ugen_ptr input, const char *address_) : Ugen(id, 0, 0) {
        // round up to multiple of BL:
        address = o2_heapify(address_);
        init_input(input);
        
        samps_stored = 0;
        frame_size = 2048;
        buffer_size = 512;
        input_chans = input->chans;
        
        odfs = O2_MALLOCNT(input_chans, LPSpectralDifferenceODF*);
        detectors = O2_MALLOCNT(input_chans, RTOnsetDetection*);
        frames.init(input_chans, false, true);
        for (int i = 0; i < input_chans; i++){
            if (old_odfs.size() > 0) odfs[i] = old_odfs.pop_back();
            else odfs[i] = new LPSpectralDifferenceODF();
            odfs[i]->init();
            
            detectors[i] = new RTOnsetDetection();
            
            frames[i] = O2_MALLOCNT(frame_size, double);
        }
    }


    ~Onset() {
        O2_FREE((char *) address);
        for (int i = 0; i < input_chans; i++){
            old_odfs.push_back(odfs[i]);
            old_detectors.push_back(detectors[i]);
            O2_FREE(frames[i]);
        }
        frames.finish();
        O2_FREE(odfs);
        O2_FREE(detectors);
    }


    void init_input(Ugen_ptr ugen) { init_param(ugen, input, input_stride); }

    const char *classname() { return Onset_name; }
    
    void real_run() {
        input_samps = input->run(current_block);
        
        if (samps_stored == frame_size){ // we have a full frame, delete first frame and shift rest
            for (int i = 0; i < input_chans; i++){
                for (int j = 0; j < frame_size - buffer_size; j++){
                    frames[i][j] = frames[i][j + buffer_size];
                }
            }
        }
        
        for (int i = 0; i < input_chans; i++){ //copy one block to each frame buffer
            for (int j = 0; j < BL; j++){
                frames[i][j + samps_stored] = *(&input_samps[i*BL + j]);
            }
        }
        
        samps_stored += BL;
        if (samps_stored != frame_size) return; //compute odf if we have full frame
        
        for (int i = 0; i < input_chans; i++){
            float odf_res = odfs[i]->process_frame(frame_size, frames[i]);
            if (detectors[i]->is_onset(odf_res)){
                o2sm_send_start();
                o2sm_add_int32(id);
                o2sm_add_int32(i); // send which channel
                o2sm_send_finish(0, address, false);
            }
        }
        samps_stored = 0;
    }
};

Vec<LPSpectralDifferenceODF*> Onset::old_odfs;
Vec<RTOnsetDetection*> Onset::old_detectors;
