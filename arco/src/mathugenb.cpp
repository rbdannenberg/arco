/* mathb -- unit generator for arco
 *
 */

#include "arcougen.h"
#include "fastrand.h"
#include "mathugenb.h"

const char *Mathb_name = "Mathb";

/* O2SM INTERFACE: /arco/mathb/new int32 id, int32 chans, 
                                  int32 op, int32 x1, int32 x2;
 */
void arco_mathb_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chans = argv[1]->i;
    int32_t op = argv[2]->i;
    int32_t x1 = argv[3]->i;
    int32_t x2 = argv[4]->i;
    // end unpack message

    ANY_UGEN_FROM_ID(x1_ugen,x1, "arco_mathb_new");
    ANY_UGEN_FROM_ID(x2_ugen,x2, "arco_mathb_new");

    new Mathb(id, chans, op, x1_ugen, x2_ugen);
}


/* O2SM INTERFACE: /arco/mathb/repl_x1 int32 id, int32 x1_id;
 */
static void arco_mathb_repl_x1(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t x1_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Mathb, mathb, id, "arco_mathb_repl_x1");
    ANY_UGEN_FROM_ID(x1, x1_id, "arco_mathb_repl_x1");
    mathb->repl_x1(x1);
}


/* O2SM INTERFACE: /arco/mathb/set_x1 int32 id, int32 chan, float val;
 */
static void arco_mathb_set_x1(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chan = argv[1]->i;
    float val = argv[2]->f;
    // end unpack message

    UGEN_FROM_ID(Mathb, mathb, id, "arco_mathb_set_x1");
    mathb->set_x1(chan, val);
}


/* O2SM INTERFACE: /arco/mathb/repl_x2 int32 id, int32 x2_id;
 */
static void arco_mathb_repl_x2(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t x2_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Mathb, mathb, id, "arco_mathb_repl_x2");
    ANY_UGEN_FROM_ID(x2, x2_id, "arco_mathb_repl_x2");
    mathb->repl_x2(x2);
}


/* O2SM INTERFACE: /arco/mathb/set_x2 int32 id, int32 chan, float val;
 */
static void arco_mathb_set_x2(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chan = argv[1]->i;
    float val = argv[2]->f;
    // end unpack message

    UGEN_FROM_ID(Mathb, mathb, id, "arco_mathb_set_x2");
    mathb->set_x2(chan, val);
}


static void mathb_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/mathb/new", "iiiii", arco_mathb_new, NULL, true,
                    true);
    o2sm_method_new("/arco/mathb/repl_x1", "ii", arco_mathb_repl_x1, NULL,
                    true, true);
    o2sm_method_new("/arco/mathb/set_x1", "iif", arco_mathb_set_x1, NULL,
                    true, true);
    o2sm_method_new("/arco/mathb/repl_x2", "ii", arco_mathb_repl_x2, NULL,
                    true, true);
    o2sm_method_new("/arco/mathb/set_x2", "iif", arco_mathb_set_x2, NULL,
                    true, true);
    // END INTERFACE INITIALIZATION

    // class initialization code from faust:
}

Initializer mathb_init_obj(mathb_init);
