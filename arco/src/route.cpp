/* route.cpp -- unit generator that passes audio through
 *
 * Roger B. Dannenberg
 * Jan 2022
 */

#include "arcougen.h"
#include "route.h"

const char *Route_name = "Route";

/* /arco/route/new int32 id, int32 chans;
 */
void arco_route_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chans = argv[1]->i;
    // end unpack message

    new Route(id, chans);
}


void arco_route_ins_rem(O2SM_HANDLER_ARGS, const char *name, bool ins_flag)
{
    int id, input;
    o2_extract_start(msg);
    O2arg_ptr ap = o2_get_next(O2_INT32); if (!ap) goto bad_args;
    id = ap->i;
    ap = o2_get_next(O2_INT32); if (!ap) goto bad_args;
    input = ap->i;
    {
        UGEN_FROM_ID(Route, route, id, name);
        ANY_UGEN_FROM_ID(ugen, input, name);
        int src, dst;
        int n = 2;
        while ((ap = o2_get_next(O2_INT32))) {
            src = ap->i;
            ap = o2_get_next(O2_INT32);
            if (!ap) break;
            dst = ap->i;
            ins_flag ? route->ins(ugen, src, dst) : route->rem(ugen, src, dst);
            n += 2;
        }
        if (n != argc) {
            goto bad_args;
        }
        return;
    }
  bad_args:
    arco_warn("/arco/route/%s: bad type string %s", name + 11, types);
}


void arco_route_ins(O2SM_HANDLER_ARGS)
{
    arco_route_ins_rem(msg, types, argv, argc, user_data,
                       "arco_route_ins", true);
}


void arco_route_rem(O2SM_HANDLER_ARGS)
{
    arco_route_ins_rem(msg, types, argv, argc, user_data,
                       "arco_route_rem", false);
}


/* /arco/route/reminput id, input; */
void arco_route_reminput(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t input = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Route, route, id, "arco_route_reminput");
    ANY_UGEN_FROM_ID(ugen, input, "arco_route_reminput");

    route->reminput(ugen);
}


static void route_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/route/new", "ii", arco_route_new, NULL, true, true);
    o2sm_method_new("/arco/route/reminput", "ii", arco_route_reminput, NULL,
                    true, true);
    // END INTERFACE INITIALIZATION
    o2sm_method_new("/arco/route/ins", NULL, arco_route_ins, NULL, false, true);
    o2sm_method_new("/arco/route/rem", NULL, arco_route_rem, NULL, false, true);
}

Initializer route_init_obj(route_init);
