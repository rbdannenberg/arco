/* route.h -- unit generator that passes audio through
 *
 * Roger B. Dannenberg
 * Jan 2022
 */

extern const char *Route_name;

class Route : public Ugen {
public:
    struct Route_input {
        Ugen_ptr input;
        int refcount;
    };


    Vec<Route_input> inputs;
    Vec< Vec<Sample_ptr> > routes;


    Route(int id, int nchans) : Ugen(id, 'a', nchans) {
        printf("Route@%p created, id %d\n", this, id);
        routes.set_size(chans, false);
        // initial routing is output all zeros:
        for (int i = 0; i < chans; i++) {
            Vec<Sample_ptr> &route = routes[i];
            route.init(1, false);
            route.set_size(1, false);
            route[0] = ugen_table[ZERO_ID]->output.get_array();
        }
    };


    ~Route() {
        for (int i = 0; i < chans; i++) {
            routes[i].finish();
        }
        for (int i = 0; i < inputs.size(); i++) {
            Ugen_ptr input = inputs[i].input;
            if (input != ugen_table[ZERO_ID]) {
                input->unref();
            }
        }
    }


    const char *classname() { return Route_name; }


    // find the index of Ugen input -- linear search
    int find(Ugen_ptr input) {
        for (int i = 0; i < inputs.size(); i++) {
            if (inputs[i].input == input) {
                return i;
            }
        }
        return -1;
    }
        

    void ins(Ugen_ptr input, int inchan, int outchan) {
        if (outchan < 0 || outchan >= chans) {
            arco_warn("Route::ins destination %d out of bounds (chans is %d)",
                      outchan, chans);
            return;
        }
        if (inchan < 0 || inchan >= input->chans) {
            arco_warn("Route::ins source %d out of bounds (input chans is %d)",
                      inchan, input->chans);
            return;
        }

        // see if route already exists (each must be unique)
        Vec<Sample_ptr> &route_vec = routes[outchan];
        Sample_ptr source = input->out_samps + BL
        
        * inchan;
        Sample_ptr *srcs = route_vec.get_array();
        int n = route_vec.size();
        for (int i = 0; i < n; i++) {
            if (source == srcs[i]) {
                return;  // route already exists
            }
        }

        // see if we have a reference to input already
        int i = find(input);
        if (i == -1) {  // add new input to our list of inputs
            Route_input *ri = inputs.append_space(1);
            ri->input = input;
            ri->refcount = 1;
            input->ref();
        } else {
            inputs[i].refcount++;
        }

        // insert the new route
        if (n == 1 && srcs[0] == ugen_table[ZERO_ID]->out_samps) {
            srcs[0] = source;  // replace the zero source with real one
        } else {
            route_vec.push_back(source);
        }
    }


    void rem(Ugen_ptr input, int inchan, int outchan) {
        if (outchan < 0 || outchan >= chans) {
            arco_warn("Route::rem destination %d out of bounds (chans is %d)",
                      outchan, chans);
            return;
        }
        if (inchan < 0 || inchan >= input->chans) {
            arco_warn("Route::rem source %d out of bounds (input chans is %d)",
                      inchan, input->chans);
            return;
        }

        // see if we have a reference to input
        int ii = find(input);
        if (ii == -1) {
            arco_warn("Route::rem specified input %p (%s) that "
                      "is not our input", input, input->classname());
            return;
        }

        // see if route already exists:
        Vec<Sample_ptr> &route_vec = routes[outchan];
        Sample_ptr source = input->out_samps + BL * inchan;
        Sample_ptr *srcs = route_vec.get_array();
        int n = route_vec.size();
        int ri;
        for (ri = 0; ri < n; ri++) {
            if (source == srcs[ri]) {
                break;  // route exists
            }
        }
        if (ri == n) {
            arco_warn("Route::rem route does not exist from %p (%s) "
                      "chan %d to output channel %d", input,
                      input->classname(), inchan, outchan);
            return;
        }

        // remove the route
        route_vec.remove(ri);
        if (route_vec.size() == 0) {  // need at least one source
            route_vec.push_back(ugen_table[ZERO_ID]->out_samps);
        }

        if (--inputs[ii].refcount == 0) {
            inputs.remove(ii);
            input->unref();
        }
    }


    void reminput(Ugen_ptr input) {
        // see if we have a reference to input
        int ii = find(input);
        if (ii == -1) {
            arco_warn("Route::reminput specified input %p (%s) that "
                      "is an input", input, input->classname());
            return;
        }

        // find all routes from this input
        for (int chan = 0; chan < chans; chan++) {
            Vec<Sample_ptr> &route_vec = routes[chan];
            for (int i = 0; i < route_vec.size(); i++) {
                Sample_ptr src = route_vec[i];
                if (src >= input->out_samps &&
                    src < input->out_samps + BL * input->chans) {
                    // route_vec[i] is a route from input to remove
                    route_vec.remove(i);
                    i--;  // use the same i on the next iteration
                }
            }
            if (route_vec.size() == 0) {
                route_vec.push_back(ugen_table[ZERO_ID]->out_samps);
            }
        }
 
        // remove input from inputs
        inputs.remove(ii);
        input->unref();
    }        
        

    void print_sources(int indent, bool print_flag) {
        for (int i = 0; i < inputs.size(); i++) {
            Ugen_ptr input = inputs[i].input;
            indent_spaces(indent);
            arco_print("routes ");
            for (int chan = 0; chan < chans; chan++) {
                for (int i = 0; i < routes[chan].size(); i++) {
                    Sample_ptr src = routes[chan][i];
                    if (src >= input->out_samps &&
                        src < input->out_samps + BL * input->chans) {
                        arco_print("%d->%d ",
                                   (src - input->out_samps) / BL, chan);
                    }
                }
            }
            arco_print("\n");
            input->print_tree(indent, print_flag, "input");
        }
    }


    void real_run() {
        bool terminated = true;
        for (int i = 0; i < inputs.size(); i++) {
            Ugen *input = inputs[i].input;
            input->run(current_block);  // bring input up-to-date
            terminated &= ((input->flags & TERMINATED) != 0);
        }
        if (terminated && (flags & CAN_TERMINATE)) {
            terminate(ACTION_TERM);
        }
        for (int i = 0; i < chans; i++) {
            Vec<Sample_ptr> &route_vec = routes[i];
            // copy the first input routed to channel i:
            block_copy(out_samps, route_vec[0]);
            // add any remaining inputs routed to channel i:
            for (int ri = 1; ri < route_vec.size(); ri++) {
                Sample_ptr src = route_vec[ri];
                for (int j = 0; j < BL; j++) {
                    out_samps[j] += *src++;
                }
            }
            out_samps += BL;
        }
    }
};
