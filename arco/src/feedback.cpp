/* feedback.cpp -- unit generator that passes audio through
 *
 * Roger B. Dannenberg
 * Jan 2022
 */

#include "arcougen.h"
#include "audioio.h"
#include "feedback.h"

const char *Feedback_name = "Feedback";

/* O2SM INTERFACE: /arco/feedback/new int32 id, int32 chans, 
                          int32 inp, int32 from, int32 gain;
 */
void arco_feedback_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chans = argv[1]->i;
    int32_t inp = argv[2]->i;
    int32_t from = argv[3]->i;
    int32_t gain = argv[4]->i;
    // end unpack message

    ANY_UGEN_FROM_ID(inp_ugen, inp, "arco_feedback_new");
    ANY_UGEN_FROM_ID(from_ugen, from, "arco_feedback_new");
    ANY_UGEN_FROM_ID(gain_ugen, gain, "arco_feedback_new");
    new Feedback(id, chans, inp_ugen, from_ugen, gain_ugen);
}


/* O2SM INTERFACE: /arco/feedback/repl_inp int32 id, int32 inp;
 */
void arco_feedback_repl_inp(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t inp = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Feedback, feedback, id, "arco_feedback_repl_inp")
    ANY_UGEN_FROM_ID(ugen, inp, "arco_feedback_repl_inp");
    feedback->repl_input(ugen);
}


/* O2SM INTERFACE: /arco/feedback/repl_from int32 id, int32 from;
 */
void arco_feedback_repl_from(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t from = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Feedback, feedback, id, "arco_feedback_repl_from")
    ANY_UGEN_FROM_ID(ugen, from, "arco_feedback_repl_from");
    feedback->repl_from(ugen);
}


/* O2SM INTERFACE: /arco/feedback/repl_gain int32 id, int32 gain;
 */
void arco_feedback_repl_gain(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t gain = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Feedback, feedback, id, "arco_feedback_repl_gain")
    ANY_UGEN_FROM_ID(ugen, gain, "arco_feedback_repl_gain");
    feedback->repl_gain(ugen);
}


/* O2SM INTERFACE: /arco/feedback/set_gain int32 id, int32 chan, float val;
 */
static void arco_feedback_set_gain(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chan = argv[1]->i;
    float val = argv[2]->f;
    // end unpack message

    UGEN_FROM_ID(Feedback, feedback, id, "arco_feedback_set_gain");
    feedback->set_gain(chan, val);
}


static void feedback_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/feedback/new", "iiiii", arco_feedback_new, NULL, true, true);
    o2sm_method_new("/arco/feedback/repl_inp", "ii", arco_feedback_repl_inp, NULL, true, true);
    o2sm_method_new("/arco/feedback/repl_from", "ii", arco_feedback_repl_from, NULL, true, true);
    o2sm_method_new("/arco/feedback/repl_gain", "ii", arco_feedback_repl_inp, NULL, true, true);
    o2sm_method_new("/arco/feedback/set_gain", "iif", arco_feedback_set_gain, NULL, true, true);
    // END INTERFACE INITIALIZATION
}

Initializer feedback_init_obj(feedback_init);
