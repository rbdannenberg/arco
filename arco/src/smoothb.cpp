/* smoothb.cpp -- unit generator that represenst c-rate parameters
 *
 * Roger B. Dannenberg
 * Jan 2022
 */

#include "arcougen.h"
#include "smoothb.h"

const char *Smoothb_name = "Smoothb";


/* O2SM INTERFACE: /arco/smoothb/new int32 id, int32 chans, float cutoff;
 */
void arco_smoothb_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chans = argv[1]->i;
    float cutoff = argv[2]->f;
    // end unpack message

    new Smoothb(id, chans, cutoff);
}


/* /arco/smoothb/newn id, cutoff, x0, x1, x2, ... xn-1
 *   create a multi-channel smoothb and initialize values
 *   this is equivalent to the sequence:
 *        /arco/smoothb/new id n cutoff
 *        /arco/smoothb/set id 0 x0
 *        /arco/smoothb/set id 1 x1
 *        ...
 *        /arco/smoothb/set id n-1 xn-1
 */
void arco_smoothb_newn(O2SM_HANDLER_ARGS)
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
        Smoothb *smoothb = new Smoothb(id, argc - 2, cutoff);
        int index = 0;
        while ((ap = o2_get_next(O2_FLOAT))) {
            smoothb->set_value(index++, ap->f);
        }
        if (index != argc - 2) {
            ugen_table[smoothb->id] = NULL;
            smoothb->unref();
            goto bad_args;
        }
        return;
    }
  bad_args:
    arco_warn("/arco/const/newn: bad type string %s", types);
}


/* /arco/smoothb/setn id, x0, x1, x2 ... xn-1
 *   set n channels of smoothb to values
 *   this is equivalent to the sequence:
 *        /arco/smoothb/set id 0 x0
 *        /arco/smoothb/set id 1 x1
 *        ...
 *        /arco/smoothb/set id n-1 xn-1
 */
void arco_smoothb_setn(O2SM_HANDLER_ARGS)
{
    argc = (int) strlen(types);
    o2_extract_start(msg);
    O2arg_ptr ap = o2_get_next(O2_INT32); if (!ap) goto bad_args;
    {
        UGEN_FROM_ID(Smoothb, smoothb, ap->i, "arco_smoothb_setn");
        
        int index = 0;
        while ((ap = o2_get_next(O2_FLOAT))) {
            smoothb->set_value(index++, ap->f);
        }
        if (index != argc - 1) {
            goto bad_args;
        }
        return;
    }
  bad_args:
    arco_warn("/arco/smoothb/setn: bad type string %s", types);
}



/* O2SM INTERFACE: /arco/smoothb/set int32 id, int32 chan, float value;
 */
void arco_smoothb_set(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chan = argv[1]->i;
    float value = argv[2]->f;
    // end unpack message

    UGEN_FROM_ID(Smoothb, smoothb, id, "arco_smoothb_cutoff");
    smoothb->set_value(chan, value);
}


/* O2SM INTERFACE: /arco/smoothb/cutoff int32 id, float cutoff;
 */
void arco_smoothb_cutoff(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    float cutoff = argv[1]->f;
    // end unpack message

    UGEN_FROM_ID(Smoothb, smoothb, id, "arco_smoothb_cutoff");
    smoothb->set_cutoff(cutoff);
}


static void smoothb_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/smoothb/new", "iif", arco_smoothb_new, NULL, true,
                    true);
    o2sm_method_new("/arco/smoothb/set", "iif", arco_smoothb_set, NULL, true,
                    true);
    o2sm_method_new("/arco/smoothb/cutoff", "if", arco_smoothb_cutoff, NULL,
                    true, true);
    // END INTERFACE INITIALIZATION
    // newn and setn have variable number of parameter:
    o2sm_method_new("/arco/smoothb/newn", NULL, arco_smoothb_newn,
                    NULL, false, false);
    o2sm_method_new("/arco/smoothb/setn", NULL, arco_smoothb_setn,
                    NULL, false, false);
}

Initializer smoothb_init_obj(smoothb_init);
