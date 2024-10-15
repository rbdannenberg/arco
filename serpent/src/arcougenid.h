/* arcougenid.h -- header for Ugen_id implementation
 *
 * Roger B. Dannenberg
 * April 2023
 */

class Ugen_id;
typedef Ugen_id *Ugen_id_ptr;

int arco_ugen_id(Ugen_id_ptr ugen_id);

class Ugen_id : public Node {
public:
    int32_t id;
    int32_t epoch;
    // ugen_id_table[i] == i means "allocated"
    // ugen_id_table[i] == -i means "free message has been send"
    // Otherwise, the entry is either on the free list or the
    // to-be-freed list.
    static int free_list;
    static int free_count;  // how many on the list?
    static int to_be_freed_list;  // list of id's collected by GC
    static int to_be_freed_count; // how many on the list?
    static int32_t ugen_id_table[UGEN_TABLE_SIZE];
    static bool initialized;
    static int32_t current_arco_epoch;
    static int max_ugen_id_used;

    void *operator new(size_t size, Machine_ptr m) {
        return m->memget(size); }


    void initialize() {
        for (int i = 10; i < UGEN_TABLE_SIZE; i++) {
            ugen_id_table[i] = i + 1;
        }
        free_list = 10; // all id's from 10 are initially free
        to_be_freed_list = -1; // empty to-be-freed list
        to_be_freed_count = 0;
        for (int i = 2; i < 10; i++) {
            ugen_id_table[i] = -1;  // fill unavailable slots with -1
        }
        ugen_id_table[UGEN_TABLE_SIZE - 1] = -1; // end-of-list at end
        free_count = UGEN_TABLE_SIZE - 10;
        initialized = true;
        printf("***** Ugen_id (re)initialized *****");
    }


    Ugen_id() {
        // find the array of ids
        if (!initialized) {
            initialize();
        }
        // head pointer is free_list
        id = free_list;
        if (id != -1) {  // list has an id
            free_list = ugen_id_table[id];
            ugen_id_table[id] = id;  // indicated "allocated"
            if (id > max_ugen_id_used) {
                max_ugen_id_used = id;
                // printf("********************************** max id %d\n", id);
            }
            free_count--;
        }
        epoch = current_arco_epoch;
        // printf("Allocated ugen %d(%d) free_list %d\n", id, epoch, free_list);
    }


    // make a Ugen_id with a known id. These are used for Zero, Zerob, 
    //     Input and Previous Output (0, 1, 2 and 3):
    Ugen_id(int id_) {
        id = id_;
        epoch = current_arco_epoch;
        assert(id < 10);
        if (!initialized) {
            initialize();
        }
        // printf("constructed Ugen_id with id %d\n", id);
    }


    static void send_free_msg(int32_t id) {
        o2_send_cmd("/arco/free", 0, "i", id);
    }


    void free_now() {
        int id = arco_ugen_id(this);
        if (id > 0) {
            send_free_msg(id);
            // now mark as freed before GC
            ugen_id_table[id] = -id;
        }            
    }


    static void free_to_be_freed(Machine_ptr mach) {
        if (!initialized) {
            return;
        }
        // give a boost to GC processing when 1/2 of id's are allocated
        // we run gc_poll() UGEN_TABLE_SIZE/(2n) times when there are n 
        // free id's, so the boost goes up in proportion to 1/n.
        if (to_be_freed_list == -1) {
            int n = Ugen_id::free_count;
            while (n < UGEN_TABLE_SIZE / 2) {
                mach->gc_poll();
                printf("gc boost %d/%d ids are free\n",
                        Ugen_id::free_count, UGEN_TABLE_SIZE);
                n += Ugen_id::free_count;
            }
        }
        while (to_be_freed_list != -1) {
            int id = to_be_freed_list;
            send_free_msg(id);
            // move from to-be-freed to freed list:
            to_be_freed_list = ugen_id_table[id]; // pop
            ugen_id_table[id] = free_list; //push
            free_list = id;
            // printf("free_all freed %d\n", id);
            free_count++;
            to_be_freed_count--;
        }
    }

    void operator delete(void *obj, unsigned int m) { abort(); }
};


int arco_ugen_id(Ugen_id_ptr ugen_id);

class Ugen_id_descriptor : public Descriptor {
public:
    long mark(SExtern_ptr obj, Machine_ptr m) { return 1; }

    size_t get_size() { return sizeof(Ugen_id); }
    
    SVal repr(SExtern_ptr obj, Machine_ptr m) {
        char msg[128];
        Ugen_id_ptr ugen = (Ugen_id_ptr) (obj->objptr);
        sprintf(msg, "<ArcoUgen %d(%d)>", ugen->id, ugen->epoch);
        return SString::create(msg, SS_NFC, m);
    }

    // garbage collector calls this when Ugen_id is free
    long free(SExtern_ptr obj, Machine_ptr m) {
        Ugen_id_ptr ugen_id = (Ugen_id_ptr) (obj->objptr);
        int id = ugen_id->id;
        if (ugen_id->epoch == Ugen_id::current_arco_epoch) {
            if (id > 0) {  // add to the to-be-freed list
                Ugen_id::ugen_id_table[id] = Ugen_id::to_be_freed_list;
                Ugen_id::to_be_freed_list = id;
                Ugen_id::to_be_freed_count++;
                // printf("GC: Ugen %d(%d) added to to-be-freed list\n",
                //        id, ugen_id->epoch);
            } else { // already explicitly freed, so add to free list
                id = -id;
                Ugen_id::ugen_id_table[id] = Ugen_id::free_list;
                Ugen_id::free_list = id;
                Ugen_id::free_count++;
                // printf("GC: Ugen %d(%d) already explicitly freed, moving to free list\n");
            }
        }   
        return 4;
    }
};


/*SER class Ugen_id PENT*/

/*SER UGEN_TABLE_SIZE = 1000 PENT*/

/*SER extern Ugen_id arco_ugen_new(Machine) PENT*/
#define arco_ugen_new(m) new(m) Ugen_id

/*SER extern Ugen_id arco_ugen_new_id(int, Machine) PENT*/
#define arco_ugen_new_id(id, m) new(m) Ugen_id(id)

/*SER int arco_ugen_id(extern Ugen_id) PENT*/
// int arco_ugen_id(Ugen_id_ptr ugen_id); (declared above)

/*SER int arco_ugen_epoch(extern Ugen_id) PENT*/
int arco_ugen_epoch(Ugen_id_ptr ugen_id);

/*SER void arco_ugen_free(extern Ugen_id) PENT*/
void arco_ugen_free(Ugen_id_ptr ugen_id);

/*SER void arco_ugen_gc(Machine) PENT*/
void arco_ugen_gc(Machine_ptr m);

/*SER int arco_ugen_reset() PENT*/
int arco_ugen_reset();

/*SER void arco_ugen_gc_info(Machine) PENT*/
void arco_ugen_gc_info(Machine_ptr m);
