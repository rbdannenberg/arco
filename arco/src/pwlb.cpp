/* pwlb -- unit generator for arco
 *
 * Roger B. Dannenberg
 * Jan 2022
 */

#include "arcougen.h"
#include "pwlb.h"

const char *Pwlb_name = "Pwlb";

/* O2SM INTERFACE: /arco/pwlb/new int32 id;
 */
void arco_pwlb_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32 id = argv[0]->i;
    // end unpack message

    new Pwlb(id);
}


/* O2SM INTERFACE: /arco/pwlb/act int32 id int32 action_id;
 *    set the action_id
 */
void arco_pwlb_act(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32 id = argv[0]->i;
    int32 action_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Pwlb, pwlb, id, "arco_pwlb_env");
    pwlb->action_id = action_id;
    printf("arco_pwlb_act: set ugen %d action_id %d\n", id, action_id);
}


/* /arco/pwlb/env id d0 y0 d1 y1 ... dn-1 [yn-1]
 */
static void arco_pwlb_env(O2SM_HANDLER_ARGS)
{
    o2_extract_start(msg);
    O2arg_ptr ap = o2_get_next(O2_INT32); if (!ap) return;
    UGEN_FROM_ID(Pwlb, pwlb, ap->i, "arco_pwlb_env");

    pwlb->points.clear();
    int index = 0;
    while ((ap = o2_get_next(O2_FLOAT))) {
        float f = ap->f;
        if ((index & 1) == 0 && f < 1) {  // even index -> duration in samples
            f = 1.0f;  // minimum segment length is 1 sample
        }
        pwlb->point(f);
        index++;
    }
    if (index & 1) {  // odd length
        pwlb->point(0.0f);  // final yn-1 is zero by default
    }
}


/* O2SM INTERFACE: /arco/pwlb/start int32 id;
 */
static void arco_pwlb_start(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32 id = argv[0]->i;
    // end unpack message

    UGEN_FROM_ID(Pwlb, pwlb, id, "arco_pwlb_start");
    pwlb->start();
}


/* O2SM INTERFACE: /arco/pwlb/decay int32 id, float d;
 */
static void arco_pwlb_decay(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32 id = argv[0]->i;
    float d = argv[1]->f;
    // end unpack message

    UGEN_FROM_ID(Pwlb, pwlb, id, "arco_pwlb_decay");
    if (d < 1.0f) d = 1.0f;
    pwlb->decay(d);
}


static void pwlb_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/pwlb/new", "i", arco_pwlb_new, NULL, true, true);
    o2sm_method_new("/arco/pwlb/act", "ii", arco_pwlb_act, NULL, true, true);
    o2sm_method_new("/arco/pwlb/start", "i", arco_pwlb_start, NULL, true, true);
    o2sm_method_new("/arco/pwlb/decay", "if", arco_pwlb_decay, NULL, true, true);
    // END INTERFACE INITIALIZATION
    // arco_pwlb_env has a variable number of parameters:
    o2sm_method_new("/arco/pwlb/env", NULL, arco_pwlb_env, NULL, false, false);

    // class initialization code from faust:
}

Initializer pwlb_init_obj(pwlb_init);
