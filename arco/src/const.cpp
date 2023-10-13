/* const.cpp -- unit generator that represenst c-rate parameters
 *
 * Roger B. Dannenberg
 * Jan 2022
 */

#include "arcougen.h"
#include "const.h"

const char *Const_name = "Const";


void Const::print_details(int indent)
{
    arco_print("[");
    bool need_comma = false;
    for (int i = 0; i < chans; i++) {
        arco_print("%s%g", need_comma ? ", " : "", output[i]);
        need_comma = true;
    }
    arco_print("]");
}


/* O2SM INTERFACE: /arco/const/new int32 id, int32 chans;
 */
void arco_const_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chans = argv[1]->i;
    // end unpack message

    new Const(id, chans);
}


/* /arco/const/newn id, x0, x1, x2 ... xn-1
 *   create an n-1-channel constant initialized to the xi
 *   this is equivalent to the sequence:
 *        /arco/const/new id n-1
 *        /arco/const/set id 0 value
 *        /arco/const/set id 1 value
 *        ...
 *        /arco/const/set id n-1 value
 */
void arco_const_newn(O2SM_HANDLER_ARGS)
{
    argc = (int) strlen(types);
    if (argc <= 1) {
        goto bad_args;
    }
    {
        o2_extract_start(msg);
        O2arg_ptr ap = o2_get_next(O2_INT32); if (!ap) goto bad_args;
        Const *const_ugen = new Const(ap->i, argc - 1);
        int index = 0;
        while ((ap = o2_get_next(O2_FLOAT))) {
            const_ugen->set_value(index++, ap->f, "arco_const_newn");
        }
        if (index != argc - 1) {
            ugen_table[const_ugen->id] = NULL;
            const_ugen->unref();
            goto bad_args;
        }
        return;
    }
  bad_args:
    arco_warn("/arco/const/newn: bad type string %s", types);
}


/* /arco/const/setn id, x0, x1, x2 ... xn-1
 *   create an n-1-channel constant initialized to the xi
 *   this is equivalent to the sequence:
 *        /arco/const/set id 0 value
 *        /arco/const/set id 1 value
 *        ...
 *        /arco/const/set id n-1 value
 */
void arco_const_setn(O2SM_HANDLER_ARGS)
{
    argc = (int) strlen(types);
    o2_extract_start(msg);
    O2arg_ptr ap = o2_get_next(O2_INT32); if (!ap) goto bad_args;
    {
        UGEN_FROM_ID(Const, const_ugen, ap->i, "arco_const_setn");
        
        int index = 0;
        while ((ap = o2_get_next(O2_FLOAT))) {
            const_ugen->set_value(index++, ap->f, "arco_const_setn");
        }
        if (index != argc - 1) {
            goto bad_args;
        }
        return;
    }
  bad_args:
    arco_warn("/arco/const/setn: bad type string %s", types);
}


/* O2SM INTERFACE: /arco/const/set int32 id, int32 chan, float value;
 */
void arco_const_set(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chan = argv[1]->i;
    float value = argv[2]->f;
    // end unpack message

    UGEN_FROM_ID(Const, ugen, id, "arco_const_set");
    ugen->set_value(chan, value, "arco_const_set");
}



static void const_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/const/new", "ii", arco_const_new, NULL, true, true);
    o2sm_method_new("/arco/const/set", "iif", arco_const_set, NULL, true,
                    true);
    // END INTERFACE INITIALIZATION
    // newn and setn have variable number of parameter:
    o2sm_method_new("/arco/const/newn", NULL, arco_const_newn,
                    NULL, false, false);
    o2sm_method_new("/arco/const/setn", NULL, arco_const_setn,
                    NULL, false, false);
}

Initializer const_init_obj(const_init);
