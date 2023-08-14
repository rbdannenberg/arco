/* pv.cpp - Phase vocoder time stretch and pitch shift
 *
 * Roger B. Dannenberg
 * Aug 2023
 * see doc/design.md: "Phase Vocoder and Pitch Shifting"
 */

#include <cmath>
void test() { int x = std::ceil(1.2); }
#include "arcougen.h"
void test2() { int x = std::ceil(1.2); }
#include "cmupv.h"
void test3() { int x = std::ceil(1.2); }
#include "resamp.h"
void test4() { int x = std::ceil(1.2); }
#include "pv.h"

const char *Pv_name = "Pv";

/* O2SM INTERFACE: /arco/pv/new int32 id, int32 chans, 
       int32 inp, float ratio, int32 fftsize, int32 hopsize, 
       int32 points, int32 mode;
 */
void arco_pv_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chans = argv[1]->i;
    int32_t inp = argv[2]->i;
    float ratio = argv[3]->f;
    int32_t fftsize = argv[4]->i;
    int32_t hopsize = argv[5]->i;
    int32_t points = argv[6]->i;
    int32_t mode = argv[7]->i;
    // end unpack message

    ANY_UGEN_FROM_ID(inp_ugen, inp, "arco_pv_new");
    new Pv(id, chans, inp_ugen, ratio, fftsize, hopsize, points, mode);
}


/* O2SM INTERFACE: /arco/pv/stretch int32 id, float stretch;
 */
void arco_pv_stretch(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    float stretch = argv[1]->f;
    // end unpack message

    UGEN_FROM_ID(Pv, pv, id, "arco_pv_stretch");
    pv->set_stretch(stretch);
}


/* O2SM INTERFACE: /arco/pv/ratio int32 id, float ratio;
 */
void arco_pv_ratio(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    float ratio = argv[1]->f;
    // end unpack message

    UGEN_FROM_ID(Pv, pv, id, "arco_pv_ratio");
    pv->set_ratio(ratio);
}


/* O2SM INTERFACE: /arco/pv/repl_inp int32 id, int32 inp_id;
 */
void arco_pv_repl_inp(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t inp_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Pv, pv, id, "arco_pv_repl_inp");
    ANY_UGEN_FROM_ID(inp, inp_id, "arco_pv_repl_inp");
    pv->repl_inp(inp);
}    


static void pv_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/pv/new", "iiifiiii", arco_pv_new, NULL, true, true);
    o2sm_method_new("/arco/pv/stretch", "if", arco_pv_stretch, NULL, true,
                     true);
    o2sm_method_new("/arco/pv/ratio", "if", arco_pv_ratio, NULL, true, true);
    o2sm_method_new("/arco/pv/repl_inp", "ii", arco_pv_repl_inp, NULL, true,
                     true);
    // END INTERFACE INITIALIZATION
}

Initializer pv_init_obj(pv_init);

