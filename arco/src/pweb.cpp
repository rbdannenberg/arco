/* pweb -- unit generator for arco
 *
 * Roger B. Dannenberg
 * Jan 2022
 */

#include "arcougen.h"
#include "pweb.h"

const char *Pweb_name = "Pweb";

/* O2SM INTERFACE: /arco/pweb/new int32 id;
 */
void arco_pweb_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    // end unpack message

    new Pweb(id);
}


/* O2SM INTERFACE: /arco/pweb/act int32 id, int32 action_id;
 *    set the action_id
 */
void arco_pweb_act(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t action_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Pweb, pweb, id, "arco_pweb_act");
    pweb->action_id = action_id;
}


/* /arco/pweb/env id d0 y0 d1 y1 ... dn-1 [yn-1]
 */
static void arco_pweb_env(O2SM_HANDLER_ARGS)
{
    o2_extract_start(msg);
    O2arg_ptr ap = o2_get_next(O2_INT32); if (!ap) return;
    UGEN_FROM_ID(Pweb, pweb, ap->i, "arco_pweb_env");

    pweb->points.clear();
    int index = 0;
    while ((ap = o2_get_next(O2_FLOAT))) {
        float f = ap->f;
        if ((index & 1) == 0) {
            if (f < 1) {  // even index -> duration in samples
                f = 1.0f;  // minimum segment length is 1 sample
            } else if (f < 0) {  // odd index -> y value
                f = 0.0f;        // y must be >= 0
            }
        }
        pweb->point(f);
        index++;
    }
    if (index & 1) {  // odd length
        pweb->point(0.0f);  // final yn-1 is zero by default
    }
}


/* O2SM INTERFACE: /arco/pweb/start int32 id;
 */
static void arco_pweb_start(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    // end unpack message

    UGEN_FROM_ID(Pweb, pweb, id, "arco_pweb_start");
    pweb->start();
}


/* O2SM INTERFACE: /arco/pweb/decay int32 id, int32 d;
 */
static void arco_pweb_decay(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t d = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Pweb, pweb, id, "arco_pweb_decay");
    if (d < 1) d = 1;
    pweb->decay(d);
}


/* O2SM INTERFACE: /arco/pweb/set int32 id, float y;
 */
static void arco_pweb_set(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    float y = argv[1]->f;
    // end unpack message

    UGEN_FROM_ID(Pweb, pweb, id, "arco_pweb_decay");
    if (y < 0) y = 0;
    pweb->set(y);
}  


static void pweb_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/pweb/new", "i", arco_pweb_new, NULL, true, true);
    o2sm_method_new("/arco/pweb/act", "ii", arco_pweb_act, NULL, true, true);
    o2sm_method_new("/arco/pweb/start", "i", arco_pweb_start, NULL, true,
                     true);
    o2sm_method_new("/arco/pweb/decay", "ii", arco_pweb_decay, NULL, true,
                     true);
    o2sm_method_new("/arco/pweb/set", "if", arco_pweb_set, NULL, true, true);
    // END INTERFACE INITIALIZATION
    // arco_pweb_env has a variable number of parameters:
    o2sm_method_new("/arco/pweb/env", NULL, arco_pweb_env, NULL, false, false);

    // class initialization code from faust:
}

Initializer pweb_init_obj(pweb_init);
