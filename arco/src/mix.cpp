/* mix.cpp -- unit generator that passes audio through
 *
 * Roger B. Dannenberg
 * Feb 2022
 */

#include "arcougen.h"
#include "const.h"
#include "mix.h"

const char *Mix_name = "Mix";

Mix::~Mix()
{
    Input *input = &inputs[0];
    for (int i = 0; i < inputs.size(); i++) {
        input->input->unref();
        input->gain->unref();
        input++;
    }
    // inputs, gains, input_strides, gain_strides are all freed
}

void Mix::print_sources(int indent, bool print)
{
    char name[80];
    Input *input = &inputs[0];
    for (int i = 0; i < inputs.size(); i++) {
        snprintf(name, 80, "input %d", i);
        input->input->print_tree(indent, print, name);
        snprintf(name, 80, "gain %d", i);
        input->gain->print_tree(indent, print, name);
        input++;
    }
}

/* O2SM INTERFACE: /arco/mix/new int32 id, int32 chans;
 */
void arco_mix_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32 id = argv[0]->i;
    int32 chans = argv[1]->i;
    // end unpack message

    new Mix(id, chans);
}


/* O2SM INTERFACE: /arco/mix/ins int32 id, int32 input, int32 gain;
 */
void arco_mix_ins(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32 id = argv[0]->i;
    int32 input = argv[1]->i;
    int32 gain = argv[2]->i;
    // end unpack message

    UGEN_FROM_ID(Mix, mix,id,  "arco_mix_ins");
    ANY_UGEN_FROM_ID(ugen, input, "arco_mix_ins");
    ANY_UGEN_FROM_ID(gain_ugen, gain, "arco_mix_ins");

    mix->ins(ugen, gain_ugen);
}

/* O2SM INTERFACE: /arco/mix/rem int32 id, int32 input;
 */
void arco_mix_rem(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32 id = argv[0]->i;
    int32 input = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Mix, mix,id,  "arco_mix_rem");
    ANY_UGEN_FROM_ID(ugen, input, "arco_mix_rem");

    mix->rem(ugen);
}

/* O2SM INTERFACE: /arco/mix/set_gain int32 id, int32 inp, float gain;
 */
void arco_mix_set_gain(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32 id = argv[0]->i;
    int32 inp = argv[1]->i;
    float gain = argv[2]->f;
    // end unpack message

    UGEN_FROM_ID(Mix, mix,id,  "arco_mix_set_gain");
    ANY_UGEN_FROM_ID(ugen, inp, "arco_mix_set_gain");

    mix->set_gain(ugen, gain);
}

/* O2SM INTERFACE: /arco/mix/repl_gain int32 id, int32 inp, int32 gain;
 */
void arco_mix_repl_gain(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32 id = argv[0]->i;
    int32 inp = argv[1]->i;
    int32 gain = argv[2]->i;
    // end unpack message

    UGEN_FROM_ID(Mix, mix,id,  "arco_mix_repl_gain");
    ANY_UGEN_FROM_ID(ugen, inp, "arco_mix_repl_gain");
    ANY_UGEN_FROM_ID(gain_ugen, gain, "arco_mix_repl_gain");

    mix->repl_gain(ugen, gain_ugen);
}

static void mix_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/mix/new", "ii", arco_mix_new, NULL, true, true);
    o2sm_method_new("/arco/mix/ins", "iii", arco_mix_ins, NULL, true, true);
    o2sm_method_new("/arco/mix/rem", "ii", arco_mix_rem, NULL, true, true);
    o2sm_method_new("/arco/mix/set_gain", "iif", arco_mix_set_gain, NULL, true, true);
    o2sm_method_new("/arco/mix/repl_gain", "iii", arco_mix_repl_gain, NULL, true, true);
    // END INTERFACE INITIALIZATION
}


Initializer mix_init_obj(mix_init);
