// multisend.h -- send a message to multiple recipients
//
// Roger B. Dannenberg
// March 2026

class Multisend_target : O2obj {
  public:
    const char *address;
    Ugen_ptr ugen_ptr;
    const char *type_string;
    Vec<O2arg> parameters;  // not including ugen_ptr->id
};

typedef Multisend_target *Multisend_target_ptr;

extern const char *Multisend_name;
// for cpp: const char *Multisend_name = "Multisend";

class Multisend : public Ugen {
  public:
    Vec<Multisend_target> targets;

    Multisend(int id) : Ugen(id, 0, 0) {
        return;
    }

    ~Multisend() {
        for (int i = 0; i < targets.size(); i++) {
            Multisend_target_ptr mstp = &targets[i];
            if (mstp->address) O2_FREE((void *) mstp->address);
            mstp->ugen_ptr->unref(&(mstp->ugen_ptr));
            if (mstp->type_string) O2_FREE((void *) mstp->type_string);
            mstp->parameters.finish();
        }
        // targets destructor calls targets.finish()
    }


    const char *classname() { return Multisend_name; }

#if ARCO_REF_DEBUG
    // for tracing tree of Ugens. Returns true with the ith child in *child
    // or false if i is too high.
    bool get_ref(int i, Ugen **child) {
        if (i < 0 || i >= targets.size()) {
            return false;
        }
        Multisend_target_ptr mstp = &targets[i];
        *child = (mstp ? mstp->ugen_ptr : NULL);
        return true;
    }
#endif


    void print_details(int indent) {
        arco_print("forwards to\n");
        for (int i = 0; i < targets.size(); i++) {
            indent_spaces(indent);
            targets[i].ugen_ptr->print(indent + 1);
        }
    }

    
    void ins(const char *types, int argc) {
        Multisend_target_ptr mstp = targets.append_space(1);
        mstp->parameters.init(argc - 3);
        mstp->parameters.set_size(argc - 3, false);
        // must include a target_id and address string
        assert(argc >= 3 && types[1] == 's' && types[2] == 'i');
        O2arg_ptr ap = o2_get_next(O2_STRING);  // get the address
        if (!ap) return;
        mstp->address = o2_heapify(ap->s);
        ap = o2_get_next(O2_INT32);  // get the target
        if (!ap) return;
        mstp->ugen_ptr = ugen_table[ap->i];
        mstp->ugen_ptr->ref();
        // types will begin with "iai" for id, addr, target, so the
        // type_string for the constructed message will start at types + 2
        mstp->type_string = o2_heapify(types + 2);  // msg address string
        for (int i = 3; i < argc; i++) {
            // only bool, int32 or float allowed for now
            if (types[i] == 'i') {
                ap = o2_get_next(O2_INT32);
            } else if (types[i] == 'f') {
                ap = o2_get_next(O2_FLOAT);
            } else if (types[i] == 'B') {
                ap = o2_get_next(O2_BOOL);
            } else {
                arco_error("Multisend cannot send message with type %c",
                           types[i]);
                return;  // type not allowed
            }
            mstp->parameters[i - 3] = *ap;
        }
    }


    void send() {
        for (int i = 0; i < targets.size(); i++) {
            Multisend_target_ptr mstp = &targets[i];
            Ugen_ptr u = mstp->ugen_ptr;
            o2sm_send_start();
            // temporarily use table[9] to map to target
            ugen_table[9] = u;
            o2sm_add_int32(9);
            // printf("Multisend::send to %s id %d\n", u->classname(), u->id);
            for (int i = 0; i < mstp->parameters.size(); i++) {
                char type_char = mstp->type_string[i + 1];
                O2arg ap = mstp->parameters[i];
                if (type_char == 'i') {
                    o2sm_add_int32(ap.i);
                } else if (type_char == 'f') {
                    o2sm_add_float(ap.f);
                } else if (type_char == 'B') {
                    o2sm_add_bool(ap.B);
                } else {
                    arco_error("Multisend cannot send message with type %c",
                               type_char);
                }
            }
            // normally, we would use o2_send_finish(), but that sends the
            // message back to the host for dispatch to arbitrary service,
            // and that is asynchronous, but we want synchronous delivery:
            O2message_ptr msg = o2_message_finish(0.0, mstp->address, true);
            if (!msg) {
                return;
            }
            o2sm_dispatch(msg);  // synchronous delivery within this process
        }
        ugen_table[9] = 0;  // return to normal
    }


    void real_run() {
        return;
    }
};
