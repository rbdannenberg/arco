/* lowpass -- unit generator for arco
 *
 * generated by f2a.py
 */

#include "arcougen.h"
#include "lowpass.h"

const char *Lowpass_name = "Lowpass";

/* O2SM INTERFACE: /arco/lowpass/new int32 id, int32 chans, int32 input, int32 cutoff;
 */
void arco_lowpass_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chans = argv[1]->i;
    int32_t input = argv[2]->i;
    int32_t cutoff = argv[3]->i;
    // end unpack message

    ANY_UGEN_FROM_ID(input_ugen, input, "arco_lowpass_new");
    ANY_UGEN_FROM_ID(cutoff_ugen, cutoff, "arco_lowpass_new");

    new Lowpass(id, chans, input_ugen, cutoff_ugen);
}


/* O2SM INTERFACE: /arco/lowpass/repl_input int32 id, int32 input_id;
 */
static void arco_lowpass_repl_input(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t input_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Lowpass, lowpass, id, "arco_lowpass_repl_input");
    ANY_UGEN_FROM_ID(input, input_id, "arco_lowpass_repl_input");
    lowpass->repl_input(input);
}


/* O2SM INTERFACE: /arco/lowpass/set_input int32 id, int32 chan, float val;
 */
static void arco_lowpass_set_input (O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chan = argv[1]->i;
    float val = argv[2]->f;
    // end unpack message

    UGEN_FROM_ID(Lowpass, lowpass, id, "arco_lowpass_set_input");
    lowpass->set_input(chan, val);
}


/* O2SM INTERFACE: /arco/lowpass/repl_cutoff int32 id, int32 cutoff_id;
 */
static void arco_lowpass_repl_cutoff(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t cutoff_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Lowpass, lowpass, id, "arco_lowpass_repl_cutoff");
    ANY_UGEN_FROM_ID(cutoff, cutoff_id, "arco_lowpass_repl_cutoff");
    lowpass->repl_cutoff(cutoff);
}


/* O2SM INTERFACE: /arco/lowpass/set_cutoff int32 id, int32 chan, float val;
 */
static void arco_lowpass_set_cutoff (O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chan = argv[1]->i;
    float val = argv[2]->f;
    // end unpack message

    UGEN_FROM_ID(Lowpass, lowpass, id, "arco_lowpass_set_cutoff");
    lowpass->set_cutoff(chan, val);
}


static void lowpass_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/lowpass/new", "iiii", arco_lowpass_new, NULL,
                    true, true);
    o2sm_method_new("/arco/lowpass/repl_input", "ii",
                    arco_lowpass_repl_input, NULL, true, true);
    o2sm_method_new("/arco/lowpass/set_input", "iif",
                    arco_lowpass_set_input, NULL, true, true);
    o2sm_method_new("/arco/lowpass/repl_cutoff", "ii",
                    arco_lowpass_repl_cutoff, NULL, true, true);
    o2sm_method_new("/arco/lowpass/set_cutoff", "iif",
                    arco_lowpass_set_cutoff, NULL, true, true);
    // END INTERFACE INITIALIZATION

    // class initialization code from faust:
}

Initializer lowpass_init_obj(lowpass_init);
