/* blendb -- unit generator for arco
 *
 * adapted from multx, Oct 2024
 *
 * blendb is a selector using a signal to select or mix 2 inputs
 *
 * The blendb parameter can be provided with an initial value like multx.
 * 
 */

#include "arcougen.h"
#include "blendb.h"

const char *Blendb_name = "Blendb";

/* O2SM INTERFACE: /arco/blendb/new int32 id, int32 chans, int32 x1, int32 x2,
                                   int32 b, int32 mode;
 */
void arco_blendb_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chans = argv[1]->i;
    int32_t x1 = argv[2]->i;
    int32_t x2 = argv[3]->i;
    int32_t b = argv[4]->i;
    int32_t mode = argv[5]->i;
    // end unpack message

    ANY_UGEN_FROM_ID(x1_ugen, x1, "arco_blendb_new");
    ANY_UGEN_FROM_ID(x2_ugen, x2, "arco_blendb_new");
    ANY_UGEN_FROM_ID(b_ugen, b, "arco_blendb_new");

    new Blendb(id, chans, x1_ugen, x2_ugen, b_ugen, mode);
}


/* O2SM INTERFACE: /arco/blendb/repl_x1 int32 id, int32 x1_id;
 */
static void arco_blendb_repl_x1(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t x1_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Blendb, blendb, id, "arco_blendb_repl_x1");
    ANY_UGEN_FROM_ID(x1, x1_id, "arco_blendb_repl_x1");
    blendb->repl_x1(x1);
}


/* O2SM INTERFACE: /arco/blendb/set_x1 int32 id, int32 chan, float val;
 */
static void arco_blendb_set_x1(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chan = argv[1]->i;
    float val = argv[2]->f;
    // end unpack message

    UGEN_FROM_ID(Blendb, blendb, id, "arco_blendb_set_x1");
    blendb->set_x1(chan, val);
}


/* O2SM INTERFACE: /arco/blendb/repl_x2 int32 id, int32 x2_id;
 */
static void arco_blendb_repl_x2(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t x2_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Blendb, blendb, id, "arco_blendb_repl_x2");
    ANY_UGEN_FROM_ID(x2, x2_id, "arco_blendb_repl_x2");
    blendb->repl_x2(x2);
}


/* O2SM INTERFACE: /arco/blendb/set_x2 int32 id, int32 chan, float val;
 */
static void arco_blendb_set_x2(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chan = argv[1]->i;
    float val = argv[2]->f;
    // end unpack message

    UGEN_FROM_ID(Blendb, blendb, id, "arco_blendb_set_x2");
    blendb->set_x2(chan, val);
}


/* O2SM INTERFACE: /arco/blendb/repl_b int32 id, int32 b_id;
 */
static void arco_blendb_repl_b(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t b_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Blendb, blendb, id, "arco_blendb_repl_b");
    ANY_UGEN_FROM_ID(b, b_id, "arco_blendb_repl_b");
    blendb->repl_b(b);
}


/* O2SM INTERFACE: /arco/blendb/set_b int32 id, int32 chan, float val;
 */
static void arco_blendb_set_b(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chan = argv[1]->i;
    float val = argv[2]->f;
    // end unpack message

    UGEN_FROM_ID(Blendb, blendb, id, "arco_blendb_set_b");
    blendb->set_b(chan, val);
}


/* O2SM INTERFACE: /arco/blendb/gain int32 id, float val;
 */
static void arco_blendb_gain(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    float val = argv[2]->f;
    // end unpack message

    UGEN_FROM_ID(Blendb, blendb, id, "arco_blendb_gain");
    blendb->gain = val;
}


/* O2SM INTERFACE: /arco/blendb/mode int32 id, int32 val;
 */
static void arco_blendb_mode(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    float val = argv[2]->i;
    // end unpack message

    UGEN_FROM_ID(Blendb, blendb, id, "arco_blendb_mode");
    blendb->mode = val;
}


static void blendb_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/blendb/new", "iiiiii", arco_blendb_new, NULL,
                    true, true);
    o2sm_method_new("/arco/blendb/repl_x1", "ii", arco_blendb_repl_x1, NULL,
                    true, true);
    o2sm_method_new("/arco/blendb/set_x1", "iif", arco_blendb_set_x1, NULL,
                    true, true);
    o2sm_method_new("/arco/blendb/repl_x2", "ii", arco_blendb_repl_x2, NULL,
                    true, true);
    o2sm_method_new("/arco/blendb/set_x2", "iif", arco_blendb_set_x2, NULL,
                    true, true);
    o2sm_method_new("/arco/blendb/repl_b", "ii", arco_blendb_repl_b, NULL,
                    true, true);
    o2sm_method_new("/arco/blendb/set_b", "iif", arco_blendb_set_b, NULL,
                    true, true);
    o2sm_method_new("/arco/blendb/gain", "iif", arco_blendb_gain, NULL,
                    true, true);
    o2sm_method_new("/arco/blendb/mode", "iif", arco_blendb_mode, NULL,
                    true, true);
    // END INTERFACE INITIALIZATION
}

Initializer blendb_init_obj(blendb_init);
