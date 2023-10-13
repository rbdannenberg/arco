// vu.cpp -- probe for vu meters
//
// Roger B. Dannenberg
// May 2023

#include "arcougen.h"
#include "zero.h"
#include "vu.h"

const char *Vu_name = "Vu";

void Vu::real_run()
{
    if (!running) {
        return;
    }
    input_samps = input->run(current_block);
    for (int chan = 0; chan < chans; chan++) {
        float peak = peaks[chan];
        for (int i = 0; i < BL; i++) {
            float samp = *input_samps++;
            if (samp > peak) {
                peak = samp;
            } else if (samp < -peak) {
                peak = -samp;
            }
        }
        peaks[chan] = peak;
    }

    // Calculate peak messages
    peak_count += BL;
    if (peak_count >= peak_window) { // runs once every peak_window samples
        // send peaks to receiver:
        o2sm_send_start();
        for (int chan = 0; chan < chans; chan++) {
            o2sm_add_float(peaks[chan]);
        }
        o2sm_send_finish(0.0, vu_reply_addr, true);
        peaks.zero();
        peak_count = 0;
    }
}


void Vu::start(char *reply_addr, float period)
{
    peak_window = (int) (period * AR);
    int len = (int) strlen(reply_addr);
    if (vu_reply_addr) {
        O2_FREE(vu_reply_addr);
    }
    vu_reply_addr = O2_MALLOCNT(len + 1, char);
    memcpy(vu_reply_addr, reply_addr, len + 1);
    running = (input != NULL);
}


/* O2SM INTERFACE: /arco/vu/start int32 id, string reply_addr, float period;
 */
static void arco_vu_start(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    char *reply_addr = argv[1]->s;
    float period = argv[2]->f;
    // end unpack message

    UGEN_FROM_ID(Vu, vu, id, "arco_vu_start");
    vu->start(reply_addr, period);
}


/* O2SM INTERFACE: /arco/vu/repl_input int32 id, int32 input_id;
 */
static void arco_vu_repl_input(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t input_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Vu, vu, id, "arco_vu_repl_input");
    ANY_UGEN_FROM_ID(input, input_id, "arco_vu_repl_input");
    vu->repl_input(input);
    //printf("vu input set to %p (%s)\n", ugen, ugen->classname());
}


/* O2SM INTERFACE: /arco/vu/new int32 id, int32 chans,
                                string reply_addr, float period;
 */
static void arco_vu_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chans = argv[1]->i;
    char *reply_addr = argv[2]->s;
    float period = argv[3]->f;
    // end unpack message

    new Vu(id, chans, reply_addr, period);
}


static void vu_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/vu/start", "isf", arco_vu_start, NULL, true, true);
    o2sm_method_new("/arco/vu/repl_input", "ii", arco_vu_repl_input, NULL,
                    true, true);
    o2sm_method_new("/arco/vu/new", "iisf", arco_vu_new, NULL, true, true);
    // END INTERFACE INITIALIZATION
}

Initializer vu_init_obj(vu_init);


