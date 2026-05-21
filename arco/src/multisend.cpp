/* multisend -- unit generator for arco to send multiple messages
 *
 */

#include "arcougen.h"
#include "multisend.h"

const char *Multisend_name = "Multisend";

/* O2SM INTERFACE: /arco/multisend/new int32 id;
 */
void arco_multisend_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    // end unpack message

    new Multisend(id);
}


/* O2SM INTERFACE: /arco/multisend/send
 */
static void arco_multisend_send(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    // end unpack message

    UGEN_FROM_ID(Multisend, multisend, id, "arco_multisend_send");
    multisend->send();
}


static void arco_multisend_ins(O2SM_HANDLER_ARGS)
{
    o2_extract_start(msg);
    O2arg_ptr ap = o2_get_next(O2_INT32);
    if (!ap) return;
    UGEN_FROM_ID(Multisend, multisend, ap->i, "arco_multisend_ins");
    multisend->ins(types, strlen(types));
}


static void multisend_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/multisend/new", "i", arco_multisend_new, NULL,
                    true, true);
    o2sm_method_new("/arco/multisend/send", "i", arco_multisend_send, NULL,
                    true, true);
    // END INTERFACE INITIALIZATION
    // arco_multisend_ins has a variable number of parameters:
    o2sm_method_new("/arco/multisend/ins", NULL, arco_multisend_ins, NULL,
                    false, false);
}

Initializer multisend_init_obj(multisend_init);
