/* smooth.cpp -- unit generator that represenst c-rate parameters
 *
 * Roger B. Dannenberg
 * Aug 2025, adapted from smoothb
 */

#include "arcougen.h"
#include "smooth.h"

const char *Smooth_name = "Smooth";


/* O2SM INTERFACE: /arco/smooth/new int32 id, int32 chans, float cutoff;
 */
void arco_smooth_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chans = argv[1]->i;
    float cutoff = argv[2]->f;
    // end unpack message

    new Smooth(id, chans, cutoff);
}


/* /arco/smooth/newn id, cutoff, x0, x1, x2, ... xn-1
 *   create a multi-channel smooth and initialize values
 *   this is equivalent to the sequence:
 *        /arco/smooth/new id n cutoff
 *        /arco/smooth/set id 0 x0
 *        /arco/smooth/set id 1 x1
 *        ...
 *        /arco/smooth/set id n-1 xn-1
 */
void arco_smooth_newn(O2SM_HANDLER_ARGS)
{
    argc = (int) strlen(types);
    if (argc <= 2) {
        goto bad_args;
    }
    {
        o2_extract_start(msg);
        O2arg_ptr ap = o2_get_next(O2_INT32); if (!ap) goto bad_args;
        int32_t id = ap->i;
        ap = o2_get_next(O2_FLOAT); if (!ap) goto bad_args;
        float cutoff = ap->f;
        Smooth *smooth = new Smooth(id, argc - 2, cutoff);
        int index = 0;
        while ((ap = o2_get_next(O2_FLOAT))) {
            smooth->set_value(index++, ap->f);
        }
        if (index != argc - 2) {
            ugen_table[smooth->id] = NULL;
            smooth->unref();
            goto bad_args;
        }
        return;
    }
  bad_args:
    arco_warn("/arco/const/newn: bad type string %s", types);
}


/* /arco/smooth/setn id, x0, x1, x2 ... xn-1
 *   set n channels of smooth to values
 *   this is equivalent to the sequence:
 *        /arco/smooth/set id 0 x0
 *        /arco/smooth/set id 1 x1
 *        ...
 *        /arco/smooth/set id n-1 xn-1
 */
void arco_smooth_setn(O2SM_HANDLER_ARGS)
{
    argc = (int) strlen(types);
    o2_extract_start(msg);
    O2arg_ptr ap = o2_get_next(O2_INT32); if (!ap) goto bad_args;
    {
        UGEN_FROM_ID(Smooth, smooth, ap->i, "arco_smooth_setn");
        
        int index = 0;
        while ((ap = o2_get_next(O2_FLOAT))) {
            smooth->set_value(index++, ap->f);
        }
        if (index != argc - 1) {
            goto bad_args;
        }
        return;
    }
  bad_args:
    arco_warn("/arco/smooth/setn: bad type string %s", types);
}



/* O2SM INTERFACE: /arco/smooth/set int32 id, int32 chan, float value;
 */
void arco_smooth_set(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chan = argv[1]->i;
    float value = argv[2]->f;
    // end unpack message

    UGEN_FROM_ID(Smooth, smooth, id, "arco_smooth_cutoff");
    smooth->set_value(chan, value);
}


/* O2SM INTERFACE: /arco/smooth/cutoff int32 id, float cutoff;
 */
void arco_smooth_cutoff(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    float cutoff = argv[1]->f;
    // end unpack message

    UGEN_FROM_ID(Smooth, smooth, id, "arco_smooth_cutoff");
    smooth->set_cutoff(cutoff);
}


static void smooth_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/smooth/new", "iif", arco_smooth_new, NULL, true,
                    true);
    o2sm_method_new("/arco/smooth/set", "iif", arco_smooth_set, NULL, true,
                    true);
    o2sm_method_new("/arco/smooth/cutoff", "if", arco_smooth_cutoff, NULL,
                    true, true);
    // END INTERFACE INITIALIZATION
    // newn and setn have variable number of parameter:
    o2sm_method_new("/arco/smooth/newn", NULL, arco_smooth_newn,
                    NULL, false, false);
    o2sm_method_new("/arco/smooth/setn", NULL, arco_smooth_setn,
                    NULL, false, false);
}

Initializer smooth_init_obj(smooth_init);
