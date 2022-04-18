/* zerob.cpp -- unit generator that passes audio through
 *
 * Roger B. Dannenberg
 * Jan 2022
 */

#include "arcougen.h"
#include "zerob.h"

const char *zerob_name = "Zerob";

/* O2SM INTERFACE: /arco/zerob/new int32 id;
 */
void arco_zerob_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32 id = argv[0]->i;
    // end unpack message

    new Zerob(id);
}


static void zerob_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/zerob/new", "i", arco_zerob_new, NULL, true, true);
    // END INTERFACE INITIALIZATION
}

Initializer zerob_init_obj(zerob_init);
