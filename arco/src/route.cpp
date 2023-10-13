/* route.cpp -- unit generator that passes audio through
 *
 * Roger B. Dannenberg
 * Jan 2022
 */

#include "arcougen.h"
#include "audioio.h"
#include "route.h"

const char *Route_name = "Route";

/* /arco/route/new id, input_id, chan0, chan1, ... chann-1
 */
void arco_route_new(O2SM_HANDLER_ARGS)
{
    argc = (int) strlen(types);
    if (argc < 2) {
        goto bad_args;
    }
    {
        o2_extract_start(msg);
        O2arg_ptr ap = o2_get_next(O2_INT32); if (!ap) goto bad_args;
        int id = ap->i;
        ap = o2_get_next(O2_INT32); if (!ap) goto bad_args;
        ANY_UGEN_FROM_ID(input, ap->i, "arco_route_new");
        Route *route = new Route(id, argc - 2, input);
        int index = 0;
        while ((ap = o2_get_next(O2_INT32))) {
            route->set_route(index++, ap->i);
        }
        if (index != argc - 2) {
            ugen_table[route->id] = NULL;
            route->unref();
            goto bad_args;
        }
        return;
    }
  bad_args:
    arco_warn("/arco/route/new: bad type string %s", types);
}


/* /arco/route/routes id, chan0, chan1, ... chann-1
 *   set output source channels to chani
 *   this is equivalent to the sequence:
 *        /arco/route/route id 0 chan0
 *        /arco/route/route id 1 chan1
 *        ...
 *        /arco/route/set id n-1 chann-1
 */
void arco_route_routes(O2SM_HANDLER_ARGS)
{
    o2_extract_start(msg);
    O2arg_ptr ap = o2_get_next(O2_INT32); if (!ap) goto bad_args;
    {
        UGEN_FROM_ID(Route, route, ap->i, "arco_route_routes");
        
        int index = 0;
        while ((ap = o2_get_next(O2_INT32))) {
            route->set_route(index++, ap->i);
        }
        if (index != argc) {
            goto bad_args;
        }
        return;
    }
  bad_args:
    arco_warn("/arco/route/routes: bad type string %s", types);
}


/* O2SM INTERFACE: /arco/route/repl_inp int32 id, int32 input_id;
 */
void arco_route_repl_inp(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t input_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Route, route, id, "arco_route_repl_inp")
    ANY_UGEN_FROM_ID(input, input_id, "arco_route_repl_inp");
    route->repl_inp(input);
}


static void route_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/route/repl_inp", "ii", arco_route_repl_inp, NULL,
                    true, true);
    // END INTERFACE INITIALIZATION
    o2sm_method_new("/arco/route/new", NULL, arco_route_new, NULL, true, true);
    o2sm_method_new("/arco/route/routes", NULL, arco_route_routes,
                    NULL, true, true);
}

Initializer route_init_obj(route_init);
