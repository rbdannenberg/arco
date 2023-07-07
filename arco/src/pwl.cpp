/* pwl -- unit generator for arco
 *
 * Roger B. Dannenberg
 * Jan 2022
 */

#include "arcougen.h"
#include "pwl.h"

const char *Pwl_name = "Pwl";

/* O2SM INTERFACE: /arco/pwl/new int32 id;
 */
void arco_pwl_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    // end unpack message

    new Pwl(id);
}


/* O2SM INTERFACE: /arco/pwl/act int32 id int32 action_id;
 *    set the action_id
 */
void arco_pwl_act(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t action_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Pwl, pwl, id, "arco_pwl_act");
    pwl->action_id = action_id;
    printf("arco_pwl_act: set ugen %d action_id %d\n", id, action_id);
}


/* /arco/pwl/env id d0 y0 d1 y1 ... dn-1 [yn-1]
 */
static void arco_pwl_env(O2SM_HANDLER_ARGS)
{
    o2_extract_start(msg);
    O2arg_ptr ap = o2_get_next(O2_INT32); if (!ap) return;
    UGEN_FROM_ID(Pwl, pwl, ap->i, "arco_pwl_env");

    pwl->points.clear();
    int index = 0;
    while ((ap = o2_get_next(O2_FLOAT))) {
        float f = ap->f;
        if ((index & 1) == 0 && f < 1) {  // even index -> duration in samples
            f = 1.0f;  // minimum segment length is 1 sample
        }
        pwl->points.push_back(f);
        index++;
    }
    if (index & 1) {  // odd length
        pwl->points.push_back(0.0);  // final yn-1 is zero by default
    }
    pwl->next_point_index = pwl->points.size();  // end of envelope
}


/* O2SM INTERFACE: /arco/pwl/start int32 id;
 */
static void arco_pwl_start(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    // end unpack message

    UGEN_FROM_ID(Pwl, pwl, id, "arco_pwl_start");
    pwl->start();
}


/* O2SM INTERFACE: /arco/pwl/decay int32 id, float d;
 */
static void arco_pwl_decay(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    float d = argv[1]->f;
    // end unpack message

    UGEN_FROM_ID(Pwl, pwl, id, "arco_pwl_decay");
    if (d < 1.0f) d = 1.0f;
    pwl->decay(d);
}


static void pwl_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/pwl/new", "i", arco_pwl_new, NULL, true, true);
    o2sm_method_new("/arco/pwl/act", "ii", arco_pwl_act, NULL, true, true);
    o2sm_method_new("/arco/pwl/start", "i", arco_pwl_start, NULL, true, true);
    o2sm_method_new("/arco/pwl/decay", "if", arco_pwl_decay, NULL, true, true);
    // END INTERFACE INITIALIZATION
    // arco_pwl_env has a variable number of parameters:
    o2sm_method_new("/arco/pwl/env", NULL, arco_pwl_env, NULL, false, false);

    // class initialization code from faust:
}

Initializer pwl_init_obj(pwl_init);
