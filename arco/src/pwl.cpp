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


/* O2SM INTERFACE: /arco/pwl/stop int32 id;
 */
static void arco_pwl_stop(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    // end unpack message

    UGEN_FROM_ID(Pwl, pwl, id, "arco_pwl_stop");
    pwl->stop();
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


/* O2SM INTERFACE: /arco/pwl/set int32 id, float y;
 */
static void arco_pwl_set(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    float y = argv[1]->f;
    // end unpack message

    UGEN_FROM_ID(Pwl, pwl, id, "arco_pwe_decay");
    if (y < 0) y = 0;
    pwl->set(y);
}


static void pwl_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/pwl/new", "i", arco_pwl_new, NULL, true, true);
    o2sm_method_new("/arco/pwl/start", "i", arco_pwl_start, NULL, true, true);
    o2sm_method_new("/arco/pwl/stop", "i", arco_pwl_stop, NULL, true, true);
    o2sm_method_new("/arco/pwl/decay", "if", arco_pwl_decay, NULL, true, true);
    o2sm_method_new("/arco/pwl/set", "if", arco_pwl_set, NULL, true, true);
    // END INTERFACE INITIALIZATION
    // arco_pwl_env has a variable number of parameters:
    o2sm_method_new("/arco/pwl/env", NULL, arco_pwl_env, NULL, false, false);

    // class initialization code from faust:
}

Initializer pwl_init_obj(pwl_init);
