/* testtone.cpp -- unit generator that passes audio through
 *
 * Roger B. Dannenberg
 * Jan 2022
 */

#include "arcougen.h"
#include "testtone.h"

const char *Testtone_name = "Testtone";

/* O2SM INTERFACE: /arco/testtone/new int32 id;
 */
void arco_testtone_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    // end unpack message

    new Testtone(id);
}


static void testtone_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/testtone/new", "i", arco_testtone_new, NULL, true, true);
    // END INTERFACE INITIALIZATION
}

Initializer testtone_init_obj(testtone_init);
