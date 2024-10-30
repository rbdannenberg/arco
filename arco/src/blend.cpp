/* blend -- unit generator for arco
 *
 * adapted from multx, Oct 2024
 *
 * blend is a selector using a signal to select or mix 2 inputs
 *
 * The blend parameter can be provided with an initial value like multx.
 * 
 * To cut down on the combinatorics of different rates, if only one of
 * x1 or x2 is audio rate, we swap them. In that case, the blend 
 * parameter b must become 1 - b, which is indicated by swap_x1_x2.
 */

#include "arcougen.h"
#include "blend.h"

const char *Blend_name = "Blend";

/* O2SM INTERFACE: /arco/blend/new int32 id, int32 chans, int32 x1, int32 x2,
                                   int32 b, float b_init, int32 mode;
 */
void arco_blend_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chans = argv[1]->i;
    int32_t x1 = argv[2]->i;
    int32_t x2 = argv[3]->i;
    int32_t b = argv[4]->i;
    float b_init = argv[5]->f;
    int32_t mode = argv[6]->i;
    // end unpack message

    ANY_UGEN_FROM_ID(x1_ugen, x1, "arco_blend_new");
    ANY_UGEN_FROM_ID(x2_ugen, x2, "arco_blend_new");
    ANY_UGEN_FROM_ID(b_ugen, b, "arco_blend_new");

    new Blend(id, chans, x1_ugen, x2_ugen, b_ugen, b_init, mode);
}


/* O2SM INTERFACE: /arco/blend/repl_x1 int32 id, int32 x1_id;
 */
static void arco_blend_repl_x1(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t x1_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Blend, blend, id, "arco_blend_repl_x1");
    ANY_UGEN_FROM_ID(x1, x1_id, "arco_blend_repl_x1");
    blend->repl_x1(x1);
}


/* O2SM INTERFACE: /arco/blend/set_x1 int32 id, int32 chan, float val;
 */
static void arco_blend_set_x1(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chan = argv[1]->i;
    float val = argv[2]->f;
    // end unpack message

    UGEN_FROM_ID(Blend, blend, id, "arco_blend_set_x1");
    blend->set_x1(chan, val);
}


/* O2SM INTERFACE: /arco/blend/repl_x2 int32 id, int32 x2_id;
 */
static void arco_blend_repl_x2(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t x2_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Blend, blend, id, "arco_blend_repl_x2");
    ANY_UGEN_FROM_ID(x2, x2_id, "arco_blend_repl_x2");
    blend->repl_x2(x2);
}


/* O2SM INTERFACE: /arco/blend/set_x2 int32 id, int32 chan, float val;
 */
static void arco_blend_set_x2(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chan = argv[1]->i;
    float val = argv[2]->f;
    // end unpack message

    UGEN_FROM_ID(Blend, blend, id, "arco_blend_set_x2");
    blend->set_x2(chan, val);
}


/* O2SM INTERFACE: /arco/blend/repl_b int32 id, int32 b_id;
 */
static void arco_blend_repl_b(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t b_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Blend, blend, id, "arco_blend_repl_b");
    ANY_UGEN_FROM_ID(b, b_id, "arco_blend_repl_b");
    blend->repl_b(b);
}


/* O2SM INTERFACE: /arco/blend/set_b int32 id, int32 chan, float val;
 */
static void arco_blend_set_b(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chan = argv[1]->i;
    float val = argv[2]->f;
    // end unpack message

    UGEN_FROM_ID(Blend, blend, id, "arco_blend_set_b");
    blend->set_b(chan, val);
}


/* O2SM INTERFACE: /arco/blend/gain int32 id, float val;
 */
static void arco_blend_gain(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    float val = argv[2]->f;
    // end unpack message

    UGEN_FROM_ID(Blend, blend, id, "arco_blend_gain");
    blend->gain = val;
}


/* O2SM INTERFACE: /arco/blend/mode int32 id, int32 mode;
 */
static void arco_blend_mode(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    float val = argv[2]->i;
    // end unpack message

    UGEN_FROM_ID(Blend, blend, id, "arco_blend_mode");
    blend->mode = val;
}


static void blend_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/blend/new", "iiiiifi", arco_blend_new, NULL,
                    true, true);
    o2sm_method_new("/arco/blend/repl_x1", "ii", arco_blend_repl_x1, NULL,
                    true, true);
    o2sm_method_new("/arco/blend/set_x1", "iif", arco_blend_set_x1, NULL,
                    true, true);
    o2sm_method_new("/arco/blend/repl_x2", "ii", arco_blend_repl_x2, NULL,
                    true, true);
    o2sm_method_new("/arco/blend/set_x2", "iif", arco_blend_set_x2, NULL,
                    true, true);
    o2sm_method_new("/arco/blend/repl_b", "ii", arco_blend_repl_b, NULL,
                    true, true);
    o2sm_method_new("/arco/blend/set_b", "iif", arco_blend_set_b, NULL,
                    true, true);
    o2sm_method_new("/arco/blend/gain", "if", arco_blend_gain, NULL,
                    true, true);
    o2sm_method_new("/arco/blend/mode", "ii", arco_blend_mode, NULL,
                    true, true);
    // END INTERFACE INITIALIZATION
}

Initializer blend_init_obj(blend_init);
