/* add.cpp -- unit generator that adds inputs
 *
 * Roger B. Dannenberg
 * Nov 2023
 */

#include "arcougen.h"
#include "add.h"

const char *Add_name = "Add";

/* O2SM INTERFACE: /arco/add/new int32 id, int32 chans;
 */
void arco_add_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chans = argv[1]->i;
    // end unpack message

    ANY_UGEN_FROM_ID(add_ugen, id, "arco_add_new");

    new Add(id, chans);
}


/* O2SM INTERFACE: /arco/add/ins int32 id, int32 x_id;
 */
static void arco_add_ins(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t x_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Add, add, id, "arco_add_ins");
    ANY_UGEN_FROM_ID(x, x_id, "arco_add_ins");
    add->ins(x);
}


/* O2SM INTERFACE: /arco/add/rem int32 id, int32 x_id;
 */
static void arco_add_rem(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t x_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Add, add, id, "arco_add_rem");
    ANY_UGEN_FROM_ID(x, x_id, "arco_add_rem");
    add->rem(x);
}



static void add_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/add/new", "ii", arco_add_new, NULL, true, true);
    o2sm_method_new("/arco/add/ins", "ii", arco_add_ins, NULL, true, true);
    o2sm_method_new("/arco/add/rem", "ii", arco_add_rem, NULL, true, true);
    // END INTERFACE INITIALIZATION
}

Initializer add_init_obj(add_init);
