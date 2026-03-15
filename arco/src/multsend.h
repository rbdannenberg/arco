// multisend.h -- send a message to multiple recipients
//
// Roger B. Dannenberg
// March 2026

class Multisend_target : O2obj {
  public:
    char *address;
    Ugen_ptr ugen_ptr;
    char *type_string;
    Vec<O2arg> parameters;  // do not include ugen_ptr->id
};

typedef Multisend_target *Multisend_target_ptr;

extern const char *Multisend_name;
// for cpp: const char *Multisend_name = "Multisend";

class Multisend : public Ugen {
  public:
    Vec<Multisend_target_ptr> targets;

    char msg_header[120];  // we prebuild an outgoing message here
    char *typestring_ptr; // where to put typestring in msg_header

    Multisend(int id) : Ugen(id, 0, 0) {
        typestring_ptr = NULL;
    }


    ~Multisend() {
        if (typestring_ptr) O2_FREE(typestring_ptr);
        for (int i = 0; i < targets.size(); i++) {
            Multisend_target_ptr mstp = targets[i];
            if (mstp->address) O2_FREE(mstp->address);
            mstp->ugen_ptr->unref();
            if (mstp->type_string) O2_FREE(mstp->type_string);
            mstp->parameters.finish();
            delete mstp;
        }
    }


    const char *classname() { return Multisend_name; }

#if ARCO_REF_DEBUG
    // for tracing tree of Ugens. Returns true with the ith child in *child
    // or false if i is too high.
    bool get_ref(int i, Ugen **child) {
        if (i < 0 || i >= targets.size()) {
            return false;
        }
        Multisend_target_ptr mstp = targets[i];
        *child = mstp ? mstp->ugen_ptr : NULL;
        return true;
    }
#endif


    void print_details(int indent) {
        arco_print("forwards to");
        for (int i = 0; i < targets.size(); i++) {
            arco_print(" %d", targets[i]->ugen_ptr->id);
        }
    }

    
    void ins(const char *types, O2arg_ptr *argv, int argc) {
        Multisend_target_ptr mstp = new Multisend_target;
        // must include a target_id and address string
        assert(argc >= 3 && types[1] == 's' && types[2] == 'i');
        mstp->address = o2_heapify(argv[1].s);
        mstp->ugen_ptr = ugen_table[argv[2].i];
        mstp->ugen_ptr->ref();
        // types will begin with "iai" for id, addr, target, so the
        // type_string for the constructed message will start at types + 2
        mstp->type_string = o2_heapify(types + 2);
        for (int i = 3; i < argc; i++) {
            // only int32 or float allowed for now
            assert(types[i] == 'i' || types[i] == 'f');
            mstp->parameters.push_back(argv[i]);
        }
        targets.push_back(mstp);
    }

    
    void send() {
        for (int i = 0; i < targets.size(); i++) {
            Multisend_target_ptr mstp = targets[i];
            Ugen_ptr u = mstp->ugen_ptr;
            o2sm_send_start();
            o2sm_add_int32(u->id);
            for (int i = 0; i < mstp->parameters.size(); i++) {
                if (mstp->type_string[i + 1] == "i") {
                    o2sm_add_int32(mstp->parameters[i].i);
                } else {
                    o2sm_add_float(mstp->parameters[i].f);
                }
            }
            o2sm_send_finish(0.0, mstp->address, true);
        }
    }


    virtual void real_run();
};
