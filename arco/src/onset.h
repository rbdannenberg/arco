/* onset.h -- onset detection unit generator
 
 definitions for frame and buffer can be found at
 https://mural.maynoothuniversity.ie/4204/1/JT_Real-Time_Detection.pdf
 */

#include "detectionfunctions.h"
#include "mq.h"
#include "onsetdetection.h"

extern const char *Onset_name;

class Onset : public Ugen {
  public:
    const char *address; // where to send messages

    Ugen_ptr input;
    int input_stride;
    Sample_ptr input_samps;
    
    int frame_size;
    int hop_size;
    int input_chans;
    int samps_stored;
    
    // one for each channel
    Vec<Sample_ptr> frames;
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
        frame_size = 256;
        hop_size = 128;
        input_chans = input->chans;
        
        odfs = O2_MALLOCNT(input_chans, LPSpectralDifferenceODF*);
        detectors = O2_MALLOCNT(input_chans, RTOnsetDetection*);
        frames.init(input_chans, false, true);
        for (int i = 0; i < input_chans; i++) {
            // reuse already allocated objects if possible:
            if (old_odfs.size() > 0) {
                odfs[i] = old_odfs.pop_back();
                detectors[i] = old_detectors.pop_back();
            } else {
                odfs[i] = new LPSpectralDifferenceODF(frame_size);
                detectors[i] = new RTOnsetDetection();
            }
            odfs[i]->init();
            frames[i] = O2_MALLOCNT(frame_size, Sample);
        }
    }


    ~Onset() {
        O2_FREE((char *) address);
        // do not delete objects, which uses a lock on the heap; instead
        // push to free lists for possible reuse
        for (int i = 0; i < input_chans; i++){
            old_odfs.push_back(odfs[i]);
            old_detectors.push_back(detectors[i]);
            O2_FREE(frames[i]);
        }
        frames.finish();
        O2_FREE(odfs);
        O2_FREE(detectors);
    }


    void init_input(Ugen_ptr ugen) {
        assert(ugen->rate == 'a');
        init_param(ugen, input, &input_stride);
    }

    const char *classname() { return Onset_name; }
    
    void real_run() {
        input_samps = input->run(current_block);
        
        if (samps_stored == frame_size){
            // we have a full frame, shift by hopsize
            for (int i = 0; i < input_chans; i++) {
                memmove(frames[i], frames[i] + hop_size,
                        hop_size * sizeof(Sample));
            }
        }
        
        for (int i = 0; i < input_chans; i++) {
            // copy one input block (a channel) to each frame buffer
            block_copy(frames[i] + samps_stored, &input_samps[i * BL]);
        }
        
        samps_stored += BL;
        if (samps_stored != frame_size) return;

        //compute odf if we have full frame
        for (int i = 0; i < input_chans; i++){
            float odf_res = odfs[i]->process_frame(frame_size, frames[i]);
            if (detectors[i]->is_onset(odf_res)) {
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
