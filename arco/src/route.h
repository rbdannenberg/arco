/* route.h -- unit generator that passes audio through
 *
 * Roger B. Dannenberg
 * Jan 2022
 */

extern const char *Route_name;

class Route : public Ugen {
public:
    Ugen_ptr input;
    int input_stride;  // not used in this Ugen
    Vec<int> routes;

    Route(int id, int nchans, Ugen_ptr input) : Ugen(id, 'a', nchans) {
        printf("Route@%p created, id %d, ugen_table[id] %p\n",
               this, id, ugen_table[id]);
        init_input(input);
        routes.set_size(chans, false);
        // initial routing is output all zeros:
        for (int i = 0; i < chans; i++) {
            routes[i] = -1;
        }
    };


    ~Route() { input->unref(); }


    const char *classname() { return Route_name; }


    virtual void print_details(int indent) {
        arco_print("routes [");
        bool need_comma = false;
        for (int i = 0; i < routes.size(); i++) {
            arco_print("%s%d", need_comma ? ", " : "", routes[i]);
        }
        arco_print("]");
    }


    void set_route(int outchan, int inchan) {
        if (!routes.bounds_check(outchan)) {
            arco_warn("Route::set_route %d out of bounds (chans is %d)",
                      outchan, chans);
        } else {
            routes[outchan] = inchan;
        }
    }
    

    void print_sources(int indent, bool print_flag) {
        input->print_tree(indent, print_flag, "input");
    }


    void repl_inp(Ugen_ptr ugen) {
        input->unref();
        init_input(ugen);
    }


    void init_input(Ugen_ptr ugen) { init_param(ugen, input, input_stride); }


    void real_run() {
        Sample_ptr src = input->run(current_block);  // bring input up-to-date
        int input_chans = input->chans;
        for (int i = 0; i < chans; i++) {
            int input_chan = routes[i];
            if (input_chan >= 0 && input_chan < input_chans) {
                block_copy(out_samps, src + BL * input_chan);
            } else {
                block_zero(out_samps);
            }
            out_samps += BL;
        }
    }
};
