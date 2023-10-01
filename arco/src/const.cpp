/* const.cpp -- unit generator that represenst c-rate parameters
 *
 * Roger B. Dannenberg
 * Jan 2022
 */

#include "arcougen.h"
#include "const.h"

const char *Const_name = "Const";


void Const::print_tree(int indent, bool print, const char *parm)
{
    if (print) {
        indent_spaces(indent);
        arco_print("%s_%d #%d (%s) [", classname(), id, refcount, parm);
        bool need_comma = false;
        for (int i = 0; i < chans; i++) {
            arco_print("%s%g", need_comma ? ", " : "", output[i]);
            need_comma = true;
        }
        arco_print("]\n");
    }
}


/* O2SM INTERFACE: /arco/const/new int32 id, int32 chans;
 */
void arco_const_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chans = argv[1]->i;
    // end unpack message

    new Const(id, chans);
}


/* O2SM INTERFACE: /arco/const/newf int32 id, float value;
 *   create a 1-channel constant initialized to a value, 
 *   this is equivalent to the sequence:
 *        /arco/const/new id 1
 *        /arco/const/set id 0 value
 */
void arco_const_newf(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    float value = argv[1]->f;
    // end unpack message

    new Const(id, 1, value);
}


/* O2SM INTERFACE: /arco/const/set int32 id, int32 chan, float value;
 */
void arco_const_set(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chan = argv[1]->i;
    float value = argv[2]->f;
    // end unpack message

    ANY_UGEN_FROM_ID(ugen, id, "arco_const_set");
    ugen->const_set(chan, value, "arco_const_set");
}



static void const_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/const/new", "ii", arco_const_new, NULL, true, true);
    o2sm_method_new("/arco/const/newf", "if", arco_const_newf, NULL, true, true);
    o2sm_method_new("/arco/const/set", "iif", arco_const_set, NULL, true, true);
    // END INTERFACE INITIALIZATION
}

Initializer const_init_obj(const_init);
