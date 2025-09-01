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
    int n = inputs.size();
    if (n > 0) {
        Input *input = &inputs[0];
        for (int i = 0; i < n; i++) {
            input->input->unref();
            input->gain->unref();
            input->prev_gain.finish();
            input++;
        }
    }
    // inputs, gains, input_strides, gain_strides are all freed
}

void Mix::print_sources(int indent, bool print_flag)
{
    char name[80];
    Input *input = inputs.get_array();
    for (int i = 0; i < inputs.size(); i++) {
        snprintf(name, 80, "input %d", i);
        input->input->print_tree(indent, print_flag, name);
        snprintf(name, 80, "gain %d", i);
        input->gain->print_tree(indent, print_flag, name);
        input++;
    }
}

/* O2SM INTERFACE: /arco/mix/new int32 id, int32 chans, int32 wrap;
 */
void arco_mix_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chans = argv[1]->i;
    int32_t wrap = argv[2]->i;
    // end unpack message

    new Mix(id, chans, wrap);
}


/* O2SM INTERFACE: /arco/mix/ins int32 id, string name, int32 input, int32 gain, 
                                           float dur, int32 mode;
 */
void arco_mix_ins(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    char *name = argv[1]->s;
    int32_t input = argv[2]->i;
    int32_t gain = argv[3]->i;
    float dur = argv[4]->f;
    int32_t mode = argv[5]->i;
    // end unpack message

    UGEN_FROM_ID(Mix, mix,id,  "arco_mix_ins");
    ANY_UGEN_FROM_ID(ugen, input, "arco_mix_ins");
    ANY_UGEN_FROM_ID(gain_ugen, gain, "arco_mix_ins");

    mix->ins(name, ugen, gain_ugen, dur, mode);
}

/* O2SM INTERFACE: /arco/mix/rem int32 id, string name, float dur, int32 mode;
 */
void arco_mix_rem(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    char *name = argv[1]->s;
    float dur = argv[2]->f;
    int32_t mode = argv[3]->i;
    // end unpack message

    UGEN_FROM_ID(Mix, mix,id,  "arco_mix_rem");

    mix->rem(name, dur, mode);
}

/* O2SM INTERFACE: /arco/mix/set_gain int32 id, string name, int32 chan, float gain;
 */
void arco_mix_set_gain(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    char *name = argv[1]->s;
    int32_t chan = argv[2]->i;
    float gain = argv[3]->f;
    // end unpack message

    UGEN_FROM_ID(Mix, mix,id,  "arco_mix_set_gain");

    mix->set_gain(name, chan, gain);
}

/* O2SM INTERFACE: /arco/mix/repl_gain int32 id, string name, int32 gain;
 */
void arco_mix_repl_gain(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    char *name = argv[1]->s;
    int32_t gain = argv[2]->i;
    // end unpack message

    UGEN_FROM_ID(Mix, mix,id,  "arco_mix_repl_gain");
    ANY_UGEN_FROM_ID(gain_ugen, gain, "arco_mix_repl_gain");

    mix->repl_gain(name, gain_ugen);
}

/* O2SM INTERFACE: /arco/mix/repl_in int32 id, string name, int32 in;
 */
void arco_mix_repl_in(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    char *name = argv[1]->s;
    int32_t in = argv[2]->i;
    // end unpack message

    UGEN_FROM_ID(Mix, mix,id,  "arco_mix_repl_in");
    ANY_UGEN_FROM_ID(in_ugen, in, "arco_mix_repl_in");

    mix->repl_in(name, in_ugen);
}

static void mix_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/mix/new", "iii", arco_mix_new, NULL, true, true);
    o2sm_method_new("/arco/mix/ins", "isiifi", arco_mix_ins, NULL, true,
                    true);
    o2sm_method_new("/arco/mix/rem", "isfi", arco_mix_rem, NULL, true, true);
    o2sm_method_new("/arco/mix/set_gain", "isif", arco_mix_set_gain, NULL,
                    true, true);
    o2sm_method_new("/arco/mix/repl_gain", "isi", arco_mix_repl_gain, NULL,
                    true, true);
    o2sm_method_new("/arco/mix/repl_in", "isif", arco_mix_repl_in, NULL,
                    true, true);
    // END INTERFACE INITIALIZATION
}


Initializer mix_init_obj(mix_init);
