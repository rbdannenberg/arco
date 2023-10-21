/* pwe -- unit generator for arco
 *
 * Roger B. Dannenberg
 * Jan 2022
 */

#include "arcougen.h"
#include "pwe.h"

const char *Pwe_name = "Pwe";

/* O2SM INTERFACE: /arco/pwe/new int32 id;
 */
void arco_pwe_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    // end unpack message

    new Pwe(id);
}


/* O2SM INTERFACE: /arco/pwe/act int32 id, int32 action_id;
 *    set the action_id
 */
void arco_pwe_act(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t action_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Pwe, pwe, id, "arco_pwe_act");
    pwe->action_id = action_id;
}


/* /arco/pwe/env id d0 y0 d1 y1 ... dn-1 [yn-1]
 */
static void arco_pwe_env(O2SM_HANDLER_ARGS)
{
    o2_extract_start(msg);
    O2arg_ptr ap = o2_get_next(O2_INT32); if (!ap) return;
    UGEN_FROM_ID(Pwe, pwe, ap->i, "arco_pwe_env");

    pwe->points.clear();
    int index = 0;
    while ((ap = o2_get_next(O2_FLOAT))) {
        float f = ap->f;
        if ((index & 1) == 0 && f < 1) {  // even index -> duration in samples
            f = 1.0f;  // minimum segment length is 1 sample
        }
        pwe->point(f);
        index++;
    }
    if (index & 1) {  // odd length
        pwe->point(0.0f);  // final yn-1 is zero by default
    }
    pwe->next_point_index = pwe->points.size();  // end of envelope
}


/* O2SM INTERFACE: /arco/pwe/start int32 id;
 */
static void arco_pwe_start(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    // end unpack message

    UGEN_FROM_ID(Pwe, pwe, id, "arco_pwe_start");
    pwe->start();
}


/* O2SM INTERFACE: /arco/pwe/decay int32 id, float d;
 */
static void arco_pwe_decay(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    float d = argv[1]->f;
    // end unpack message

    UGEN_FROM_ID(Pwe, pwe, id, "arco_pwe_decay");
    if (d < 1.0f) d = 1.0f;
    pwe->decay(d);
}


/* O2SM INTERFACE: /arco/pwe/set int32 id, float y;
 */
static void arco_pwe_set(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    float y = argv[1]->f;
    // end unpack message

    UGEN_FROM_ID(Pwe, pwe, id, "arco_pwe_decay");
    if (y < 0) y = 0;
    pwe->set(y);
}  


static void pwe_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/pwe/new", "i", arco_pwe_new, NULL, true, true);
    o2sm_method_new("/arco/pwe/act", "ii", arco_pwe_act, NULL, true, true);
    o2sm_method_new("/arco/pwe/start", "i", arco_pwe_start, NULL, true, true);
    o2sm_method_new("/arco/pwe/decay", "if", arco_pwe_decay, NULL, true, true);
    o2sm_method_new("/arco/pwe/set", "if", arco_pwe_set, NULL, true, true);
    // END INTERFACE INITIALIZATION
    // arco_pwe_env has a variable number of parameters:
    o2sm_method_new("/arco/pwe/env", NULL, arco_pwe_env, NULL, false, false);

    // class initialization code from faust:
}

Initializer pwe_init_obj(pwe_init);
