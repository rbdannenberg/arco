//
//  spectralcentroid.cpp
//
//

#include "arcougen.h"
#include "spectralcentroid.h"


const char *SpectralCentroid_name = "SpectralCentroid";

void SpectralCentroid::real_run()
{
    input_samps = input->run(current_block);
    
    // Note: processAudioFrame assumes input_samps has length BL, so it only processes the first channel of input and the rest are ignored.
    
    fftcalc.processAudioFrame(input_samps);
    if (fftcalc.isReady()) { // only runs if we have enough samples
        
        
        float* magnSpec = fftcalc.getMagnitudeSpectrum();
        float* FFTFreqs = fftcalc.getFFTFrequencies();
        
        float sum = 0.0f, weightedSum = 0.0f;
        
        for (int i = 0; i < fftcalc.bufferSize / 2 + 1; i ++) {
            sum += magnSpec[i];
            weightedSum += magnSpec[i] * FFTFreqs[i];
        }
        
        // Spectral centroid = weightedSum / sum
        float result = (sum != 0.0f) ? (weightedSum / sum) : 0.0f;
        
        o2sm_send_start();
        o2sm_add_float(result);
        o2sm_send_finish(0, cd_reply_addr, false);
    }
    
}


void SpectralCentroid::start(const char *reply_addr) {
    if (cd_reply_addr) {
        O2_FREE(cd_reply_addr);
    }
    cd_reply_addr = O2_MALLOCNT(strlen(reply_addr) + 1, char);
    strcpy(cd_reply_addr, reply_addr);
}

/* O2SM INTERFACE: /arco/spectralcentroid/start int32 id, string reply_addr;
 */
static void arco_spectralcentroid_start(O2SM_HANDLER_ARGS)
{

    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    char *reply_addr = argv[1]->s;
    // end unpack message

    UGEN_FROM_ID(SpectralCentroid, spectralcentroid, id, "arco_spectralcentroid_start");
    spectralcentroid->start(reply_addr);
}


/* O2SM INTERFACE: /arco/spectralcentroid/repl_input int32 id, int32 input_id;
 */
static void arco_spectralcentroid_repl_input(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t input_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(SpectralCentroid, spectralcentroid, id, "arco_spectralcentroid_repl_input");
    ANY_UGEN_FROM_ID(input, input_id, "arco_spectralcentroid_repl_input");
    spectralcentroid->repl_input(input);
    // printf("spectralcentroid input set to %p (%s)\n", ugen, ugen->classname());
}



/* O2SM INTERFACE: /arco/spectralcentroid/new int32 id, int32 chans, string reply_addr;
 */
static void arco_spectralcentroid_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    char *reply_addr = argv[1]->s;
    // end unpack message

    new SpectralCentroid(id, reply_addr);
}


static void spectralcentroid_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/spectralcentroid/start", "is", arco_spectralcentroid_start,
                    NULL, true, true);
    o2sm_method_new("/arco/spectralcentroid/repl_input", "ii",
                    arco_spectralcentroid_repl_input, NULL, true, true);
    o2sm_method_new("/arco/spectralcentroid/new", "is", arco_spectralcentroid_new,
                    NULL, true, true);
    // END INTERFACE INITIALIZATION
}



Initializer spectralcentroid_init_obj(spectralcentroid_init);

