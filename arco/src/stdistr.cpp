/* stdistr.cpp -- unit generator that distributes inputs across stereo field
 *
 * Roger B. Dannenberg
 * Nov 2023
 */

#include "arcougen.h"
#include "stdistr.h"

const char *Stdistr_name = "Stdistr";

/* O2SM INTERFACE: /arco/stdistr/new int32 id, int32 n, float width;
 */
void arco_stdistr_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t n = argv[1]->i;
    float width = argv[2]->f;
    // end unpack message

    new Stdistr(id, n, width);
}


/* O2SM INTERFACE: /arco/stdistr/gain int32 id, float gain;
 */
static void arco_stdistr_gain(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    float gain = argv[1]->f;
    // end unpack message

    UGEN_FROM_ID(Stdistr, stdistr, id, "arco_stdistr_ins");
    stdistr->set_gain(gain);
}


/* O2SM INTERFACE: /arco/stdistr/width int32 id, float width;
 */
static void arco_stdistr_width(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    float width = argv[1]->f;
    // end unpack message

    UGEN_FROM_ID(Stdistr, stdistr, id, "arco_stdistr_ins");
    stdistr->set_width(width);
}


/* O2SM INTERFACE: /arco/stdistr/ins int32 id, int32 index, int32 x_id;
 */
static void arco_stdistr_ins(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t index = argv[1]->i;
    int32_t x_id = argv[2]->i;
    // end unpack message

    UGEN_FROM_ID(Stdistr, stdistr, id, "arco_stdistr_ins");
    ANY_UGEN_FROM_ID(x, x_id, "arco_stdistr_ins");
    stdistr->ins(index, x);
}


/* O2SM INTERFACE: /arco/stdistr/rem int32 id, int32 index;
 */
static void arco_stdistr_rem(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t index = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Stdistr, stdistr, id, "arco_stdistr_rem");
    stdistr->rem(index);
}



static void stdistr_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/stdistr/new", "iif", arco_stdistr_new, NULL,
                    true, true);
    o2sm_method_new("/arco/stdistr/gain", "if", arco_stdistr_gain, NULL,
                    true, true);
    o2sm_method_new("/arco/stdistr/width", "if", arco_stdistr_width, NULL,
                    true, true);
    o2sm_method_new("/arco/stdistr/ins", "iii", arco_stdistr_ins, NULL,
                    true, true);
    o2sm_method_new("/arco/stdistr/rem", "ii", arco_stdistr_rem, NULL, true,
                    true);
    // END INTERFACE INITIALIZATION
}

Initializer stdistr_init_obj(stdistr_init);
