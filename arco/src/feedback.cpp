/* feedback.cpp -- unit generator that passes audio through
 *
 * Roger B. Dannenberg
 * Jan 2022
 */

#include "arcougen.h"
#include "feedback.h"

const char *Feedback_name = "Feedback";

/* O2SM INTERFACE: /arco/feedback/new int32 id, int32 chans, 
                          int32 input_id, int32 from_id, int32 gain_id;
 */
void arco_feedback_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chans = argv[1]->i;
    int32_t input_id = argv[2]->i;
    int32_t from_id = argv[3]->i;
    int32_t gain_id = argv[4]->i;
    // end unpack message

    ANY_UGEN_FROM_ID(input, input_id, "arco_feedback_new");
    ANY_UGEN_FROM_ID(from, from_id, "arco_feedback_new");
    ANY_UGEN_FROM_ID(gain, gain_id, "arco_feedback_new");
    new Feedback(id, chans, input, from, gain);
}


/* O2SM INTERFACE: /arco/feedback/repl_input int32 id, int32 input_id;
 */
void arco_feedback_repl_input(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t input_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Feedback, feedback, id, "arco_feedback_repl_input")
    ANY_UGEN_FROM_ID(input, input_id, "arco_feedback_repl_input");
    feedback->repl_input(input);
}


/* O2SM INTERFACE: /arco/feedback/repl_from int32 id, int32 from_id;
 */
void arco_feedback_repl_from(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t from_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Feedback, feedback, id, "arco_feedback_repl_from")
    ANY_UGEN_FROM_ID(from, from_id, "arco_feedback_repl_from");
    feedback->repl_from(from);
}


/* O2SM INTERFACE: /arco/feedback/repl_gain int32 id, int32 gain_id;
 */
void arco_feedback_repl_gain(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t gain_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Feedback, feedback, id, "arco_feedback_repl_gain")
    ANY_UGEN_FROM_ID(gain, gain_id, "arco_feedback_repl_gain");
    feedback->repl_gain(gain);
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
    o2sm_method_new("/arco/feedback/new", "iiiii", arco_feedback_new, NULL,
                    true, true);
    o2sm_method_new("/arco/feedback/repl_input", "ii",
                    arco_feedback_repl_input, NULL, true, true);
    o2sm_method_new("/arco/feedback/repl_from", "ii", arco_feedback_repl_from,
                    NULL, true, true);
    o2sm_method_new("/arco/feedback/repl_gain", "ii", arco_feedback_repl_gain,
                    NULL, true, true);
    o2sm_method_new("/arco/feedback/set_gain", "iif", arco_feedback_set_gain,
                    NULL, true, true);
    // END INTERFACE INITIALIZATION
}

Initializer feedback_init_obj(feedback_init);
