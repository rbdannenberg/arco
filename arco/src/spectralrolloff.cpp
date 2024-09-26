//
//  spectralrolloff.cpp
//
//

#include "arcougen.h"
#include "spectralrolloff.h"


const char *SpectralRolloff_name = "SpectralRolloff";

void SpectralRolloff::real_run()
{
    // Value for constant input is close to zero.
    // but value for sine wave input fluctuates fast?
    input_samps = input->run(current_block);
    
//    for (int i = 0; i < BL; i ++) {
//        input_samps[i] = sin(2.0 * M_PI * 440.0 * testing_cnt / AR);
//        
//        testing_cnt++;
//    }
    
    // Note: processAudioFrame assumes input_samps has length BL, so it only processes the first channel of input and the rest are ignored.
    fftcalc.processAudioFrame(input_samps);
    
    
    if (fftcalc.isReady()) { // only runs if we have enough samples
        
        
        float* magnSpec = fftcalc.getMagnitudeSpectrum();
        float* FFTFreqs = fftcalc.getFFTFrequencies();
        
        float sum = 0.0f;
        
        for (int i = 0; i < fftcalc.bufferSize / 2 + 1; i++) {
            sum += magnSpec[i];
        }
        
        float cumulativeEnergy = 0.0;
        float thresholdEnergy = threshold * sum;
        int rolloffBin = 0;
        
        for (int i = 0; i < fftcalc.bufferSize / 2 + 1; i++) {
            cumulativeEnergy += magnSpec[i];
            if (cumulativeEnergy >= thresholdEnergy) {
                rolloffBin = i;
                break;
            }
        }
        
        float result = FFTFreqs[rolloffBin];
        
        o2sm_send_start();
        o2sm_add_float(result);
        o2sm_send_finish(0, cd_reply_addr, false);
    }
    
}


void SpectralRolloff::start(const char *reply_addr) {
    if (cd_reply_addr) {
        O2_FREE(cd_reply_addr);
    }
    cd_reply_addr = O2_MALLOCNT(strlen(reply_addr) + 1, char);
    strcpy(cd_reply_addr, reply_addr);
}

/* O2SM INTERFACE: /arco/spectralrolloff/start int32 id, string reply_addr;
 */
static void arco_spectralrolloff_start(O2SM_HANDLER_ARGS)
{

    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    char *reply_addr = argv[1]->s;
    // end unpack message

    UGEN_FROM_ID(SpectralRolloff, spectralrolloff, id, "arco_spectralrolloff_start");
    spectralrolloff->start(reply_addr);
}


/* O2SM INTERFACE: /arco/spectralrolloff/repl_input int32 id, int32 input_id;
 */
static void arco_spectralrolloff_repl_input(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t input_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(SpectralRolloff, spectralrolloff, id, "arco_spectralrolloff_repl_input");
    ANY_UGEN_FROM_ID(input, input_id, "arco_spectralrolloff_repl_input");
    spectralrolloff->repl_input(input);
    // printf("spectralrolloff input set to %p (%s)\n", ugen, ugen->classname());
}



/* O2SM INTERFACE: /arco/spectralrolloff/new int32 id, int32 chans, string reply_addr, float threshold;
 */
static void arco_spectralrolloff_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    char *reply_addr = argv[1]->s;
    float threshold = argv[2]->f;
    // end unpack message

    new SpectralRolloff(id, reply_addr, threshold);
}


static void spectralrolloff_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/spectralrolloff/start", "is", arco_spectralrolloff_start,
                    NULL, true, true);
    o2sm_method_new("/arco/spectralrolloff/repl_input", "ii",
                    arco_spectralrolloff_repl_input, NULL, true, true);
    o2sm_method_new("/arco/spectralrolloff/new", "isf", arco_spectralrolloff_new,
                    NULL, true, true);
    // END INTERFACE INITIALIZATION
}



Initializer spectralrolloff_init_obj(spectralrolloff_init);

