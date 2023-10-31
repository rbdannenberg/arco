/* ugen.cpp -- Unit Generator
 *
 * Roger B. Dannenberg
 * Dec 2021
 */

#include "arcougen.h"
#include "const.h"
#include "audioio.h"

Vec<Ugen_ptr> ugen_table;
int ugen_table_free_list = 1;
char control_service_addr[64] = "";
int control_service_addr_len = 0;

// safely copy ctrlservice to control_service_addr -- this is who we
//   send action messages to.  Allow 20 bytes for the rest of the address,
//   and 3 bytes for "!", "/", and EOS. Result is "!<ctrlservice>/"
int set_control_service(const char *ctrlservice)
{
    const size_t csalen = strlen(ctrlservice);
    if (csalen + 23 > sizeof(control_service_addr)) {
        arco_warn("Control service name is too long: %s.",
                  control_service_addr);
        control_service_addr[0] = 0;
        control_service_addr_len = 0;
        return 1;  // error return
    }
    control_service_addr_len = (int) (csalen + 2);
    control_service_addr[0] = '!';
    strcpy(control_service_addr + 1, ctrlservice);
    strcat(control_service_addr, "/");
    return 0;  // normal, no error
}


Initializer *initializer_list = NULL;

Initializer::Initializer(void (*f)())
{
    next = initializer_list;
    fn = f;
    initializer_list = this;
}


void Initializer::init()
{
    Initializer *d = initializer_list;
    while (d) {
        (*(d->fn))();
        d = d->next;
    }
}


void Ugen::unref() {
    refcount--;
    printf("Ugen::unref id %d %s new refcount %d\n",
           id, classname(), refcount);
    if (refcount == 0) {
        delete this;
    }
}


void Ugen::indent_spaces(int indent)
{
    for (int i = 0; i < indent; i++) {
        arco_print("  ");
    }
}


void Ugen::print(int indent, const char *param) {
    arco_print("%s_%d(%s) refs %d chans %d ",
               classname(), id, param, refcount, chans);
    print_details(indent);
    arco_print("\n");
}    


// this method runs a mark and print algorithm when print is true
// it simply unmarks everything when print is false
//
void Ugen::print_tree(int indent, bool print_flag, const char *param)
{
    if (print_flag) {
        if (flags & UGEN_MARK) {
            return;  // cut off the search; we've been here before
        }
        indent_spaces(indent);
        print(indent, param);
        flags |= UGEN_MARK;
    } else {
        if (!(flags & UGEN_MARK)) {
            return;  // cut off the search; we've been here before
        }
        flags &= ~UGEN_MARK;  // clear flag
    }
    print_sources(indent + 1, print_flag);
}


void Ugen::const_set(int chan, Sample x, const char *from)
// Assume this is a Const. Set channel chan of the const output to x.
// Do nothing but print warnings if this is not a Const ugen.
// Normally, this is called to implement a "set_input" or in general
// "set_xxxx" where xxxx is a signal input to another ugen, so from
// is a string name of the method, e.g. "Upsample::set_input".
{
    if (rate != 'c') {
        arco_warn("%s: ugen (%d) is not a Const\n", from, id);
    } else {
        ((Const *) this)->set_value(chan, x, from);
    }
}



/* /arco/free id id id ...
 */
void arco_free(O2SM_HANDLER_ARGS)
{
    o2_extract_start(msg);
    O2arg_ptr ap;
    while ((ap = o2_get_next(O2_INT32))) {
        int id = ap->i;
        ANY_UGEN_FROM_ID(ugen, id, "arco_free");
        if (ugen) {
            // printf("arco_free handler freeing %d #%d\n", id, ugen->refcount);
            ugen_table[id] = NULL;
            // special case: check for run set or output set references
            if (ugen->flags & (IN_RUN_SET | IN_OUTPUT_SET)) {
                aud_forget(id);
            }
            printf("arco_free %d (%s)\n", id, ugen->classname());
            ugen->unref();
        }
    }
}


void ugen_initialize()
{
    ugen_table.init(UGEN_TABLE_SIZE, true);  // fill with zero
    Initializer::init();  // add all message handlers to O2sm

    o2sm_method_new("/arco/free", NULL, arco_free, NULL, false, false);
}


// Look up the Ugen associated with id: if id is out of bounds or
// there is no ugen, or the ugen has an unexpected class, return NULL.
// If the class of the Ugen does not matter, pass NULL for classname.
Ugen_ptr id_to_ugen(int32_t id, const char *classname, const char *operation)
{
    if (!ugen_table.bounds_check(id)) {
        arco_error("Bad ugen index %d in %s\n", id, operation);
        return NULL;
    }
    Ugen_ptr ugen = ugen_table[id];
    if (!ugen) {
        arco_error("%s uninitialized id %d, ignored\n", operation, id);
    } else if (classname && (ugen->classname() != classname)) {
        arco_error("%s id %d found a %s Ugen instead of %s, ignored\n",
                   operation, id, ugen->classname(), classname);
        ugen = NULL;
    }
    return ugen;
}
