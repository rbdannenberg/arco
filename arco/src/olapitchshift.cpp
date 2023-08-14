/* olapitchshift.cpp - overlap/add pitch shift unit generator
 *
 * Roger B. Dannenberg
 *
 * see olapitchshift.h for details
 */

#include "arcougen.h"
#include "olapitchshift.h"

const char *Ola_pitch_shift_name = "Ola_pitch_shift";

/* O2SM INTERFACE: /arco/olaps/new int32 id, int32 chans, 
       int32 inp, float ratio, float xfade, float windur;
 */
void arco_olaps_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chans = argv[1]->i;
    int32_t inp = argv[2]->i;
    float ratio = argv[3]->f;
    float xfade = argv[4]->f;
    float windur = argv[5]->f;
    // end unpack message

    ANY_UGEN_FROM_ID(inp_ugen, inp, "arco_olaps_new");
    new Ola_pitch_shift(id, chans, inp_ugen, ratio, xfade, windur);
}


/* O2SM INTERFACE: /arco/olaps/ratio int32 id, float ratio;
 */
void arco_olaps_ratio(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    float ratio = argv[1]->f;
    // end unpack message

    UGEN_FROM_ID(Ola_pitch_shift, olaps, id, "arco_olaps_ratio");
    olaps->set_ratio(ratio);
}


/* O2SM INTERFACE: /arco/olaps/xfade int32 id, float xfade;
 */
void arco_olaps_xfade(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    float xfade = argv[1]->f;
    // end unpack message

    UGEN_FROM_ID(Ola_pitch_shift, olaps, id, "arco_olaps_xfade");
    olaps->set_xfade(xfade);
}


/* O2SM INTERFACE: /arco/olaps/windur int32 id, float windur;
 */
void arco_olaps_windur(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    float windur = argv[1]->f;
    // end unpack message

    UGEN_FROM_ID(Ola_pitch_shift, olaps, id, "arco_olaps_windur");
    olaps->set_windur(windur);
}


/* O2SM INTERFACE: /arco/olaps/repl_inp int32 id, int32 inp_id;
 */
void arco_olaps_repl_inp(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t inp_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Ola_pitch_shift, olaps, id, "arco_olaps_repl_inp");
    ANY_UGEN_FROM_ID(inp, inp_id, "arco_olaps_repl_inp");
    olaps->repl_inp(inp);
}    


static void olaps_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/olaps/new", "iiifff", arco_olaps_new, NULL, true,
                     true);
    o2sm_method_new("/arco/olaps/ratio", "if", arco_olaps_ratio, NULL, true,
                     true);
    o2sm_method_new("/arco/olaps/xfade", "if", arco_olaps_xfade, NULL, true,
                     true);
    o2sm_method_new("/arco/olaps/windur", "if", arco_olaps_windur, NULL, true,
                     true);
    // END INTERFACE INITIALIZATION
}

Initializer olaps_init_obj(olaps_init);

