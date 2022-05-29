/* zero.cpp -- unit generator that passes audio through
 *
 * Roger B. Dannenberg
 * Jan 2022
 */

const char *Zero_name = "Zero";

#include "arcougen.h"
#include "zero.h"

/* O2SM INTERFACE: /arco/zero/new int32 id;
 */
void arco_zero_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    // end unpack message

    new Zero(id);
}


static void zero_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/zero/new", "i", arco_zero_new, NULL, true, true);
    // END INTERFACE INITIALIZATION
}

Initializer zero_init_obj(zero_init);
