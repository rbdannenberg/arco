/* dnsampleb -- unit generator for arco
 *
 * Roger B. Dannenberg
 * Oct 2023
 */

#include "arcougen.h"
#include "dnsampleb.h"

const char *Dnsampleb_name = "Dnsampleb";

Dnsampleb_method dnsampleb_methods[] = {
    &Dnsampleb::dnsample_basic,
    &Dnsampleb::dnsample_avg,
    &Dnsampleb::dnsample_peak,
    &Dnsampleb::dnsample_rms,
    &Dnsampleb::dnsample_power,
    &Dnsampleb::dnsample_lowpass,
    &Dnsampleb::dnsample_lowpass };

/* O2SM INTERFACE: /arco/dnsampleb/new int32 id, int32 chans, 
                                       int32 input, int32 mode;
 */
void arco_dnsample_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chans = argv[1]->i;
    int32_t input = argv[2]->i;
    int32_t mode = argv[3]->i;
    // end unpack message

    ANY_UGEN_FROM_ID(ugen, input, "arco_dnsample_new");

    new Dnsampleb(id, chans, ugen, mode);
}

    
/* O2SM INTERFACE: /arco/dnsampleb/repl_input int32 id, int32 input_id;
 */
static void arco_dnsampleb_repl_input(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t input_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Dnsampleb, dnsampleb, id, "arco_dnsampleb_repl_input");
    ANY_UGEN_FROM_ID(input, input_id, "arco_dnsampleb_repl_input");
    dnsampleb->repl_input(input);
}


/* O2SM INTERFACE: /arco/dnsampleb/cutoff int32 id, float cutoff;
 */
void arco_dnsampleb_cutoff(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    float cutoff = argv[1]->f;
    // end unpack message

    UGEN_FROM_ID(Dnsampleb, dnsampleb, id, "arco_dnsampleb_cutoff");
    dnsampleb->set_cutoff(cutoff);
}


/* O2SM INTERFACE: /arco/dnsampleb/mode int32 id, int32 mode;
 */
void arco_dnsampleb_mode(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t mode = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Dnsampleb, dnsampleb, id, "arco_dnsampleb_mode");
    dnsampleb->set_mode(mode);
}


void dnsampleb_initialize()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/dnsampleb/new", "iiii", arco_dnsample_new, NULL,
                    true, true);
    o2sm_method_new("/arco/dnsampleb/repl_input", "ii",
                    arco_dnsampleb_repl_input, NULL, true, true);
    o2sm_method_new("/arco/dnsampleb/cutoff", "if", arco_dnsampleb_cutoff,
                    NULL, true, true);
    o2sm_method_new("/arco/dnsampleb/mode", "ii", arco_dnsampleb_mode, NULL,
                    true, true);
    // END INTERFACE INITIALIZATION
}
