/* arcougenid.h -- header for Ugen_id implementation
 *
 * Roger B. Dannenberg
 * April 2023, revised March 2026
 */
/*
   Serpent extern objects have a 64-bit objptr, which is enough storage
   for a Ugen_id, but the objptr is immutable in Serpent, so it will have
   to point to a memory location representing the Ugen_id. We'd like to avoid
   malloc, which can have priority inversion problems, but we already have
   ugen_id_table of 32-bit ids.
 
   What about epochs? We need epoch so that we can invalidate all outstanding
   Ugen_ids in the Serpent heap by incrementing the current epoch. Since epoch
   is associated with the Ugen_id and never changes (that's the point!), we
   can put the epoch in half the objptr and use the other half for the ID.
 
   Then, in the ugen_id_table, we can store the mutable state and use the
   remaining bits for links to form the linked free and to-be-freed lists.
 
   There are only two state, so we will use the sign bit. The remainder is
   used for links.  This is tricky in that we need an end-of-list marker.
   This could be -1, but the sign bit is taken for state, or it could be
   0, but 0 is an ID. However, 0 is a "special" ID for the Zero Ugen,
   which is never freed, so we use 0 for end-of-list AND for the Zero Ugen
   ID, and we just have to be careful.  (Note: a previous implementation
   used the lower-order 2 bits for state and ignored the sign bit, but had
   to shift IDs by 2 bits to extract the link or index field, so that is
   an alternate implementation option.)

   We need some macros to retrieve epoch, id, and state.
 
 */

typedef int64_t Ugen_id;
typedef Ugen_id *Ugen_id_ptr;  // just a generic pointer. We never dereference
                               // it, but only use the pointer to store info

enum Ugen_id_state {
    ON_LIST = 0,
    FREE_SENT_NO_GC = 1 };

#define UGEN_END_OF_LIST 0

#define UGEN_IN_RANGE(i) ((i >= 0) && (i < UGEN_TABLE_SIZE))
#define UGEN_IDX2ID(i) (ugen_id_table[i] & 0x7fffffff)  // clear sign bit
#define UGEN_IDX2STATE(i) (((uint32_t) ugen_id_table[i]) >> 31)  // sign bit
#define UGEN_EPOCH(p) (((uint64_t) p) >> 32)
#define UGEN_ID(p) (((int64_t) p) & 0xffffffff)
#define UGEN_STATE(p) UGEN_IDX2STATE(UGEN_IDX(p))
#define UGEN_LINK(i) (i)
#define UGEN_FREE_SENT_NO_GC(i) (-(i))

class Ugen_id_descriptor : public Descriptor {
public:
    long mark(SExtern_ptr obj, Machine_ptr m) { return 1; }

    SVal repr(SExtern_ptr obj, Machine_ptr m);

    // garbage collector calls this when Ugen_id is free
    long free(SExtern_ptr obj, Machine_ptr m);
};


/*SER UGEN_TABLE_SIZE = 5000 PENT*/

/*SER class Ugen_id PENT*/

int64_t make_ugen_id();
/*SER extern Ugen_id arco_ugen_new() PENT*/
#define arco_ugen_new(m) ((Ugen_id *) make_ugen_id())

int64_t make_ugen_id_from(int id);
/*SER extern Ugen_id arco_ugen_new_id(int) PENT*/
#define arco_ugen_new_id(id) ((Ugen_id *) make_ugen_id_from(id))

/*SER int arco_ugen_id(extern Ugen_id) PENT*/
int arco_ugen_id(Ugen_id_ptr ugen_id);

/*SER int arco_ugen_epoch(extern Ugen_id) PENT*/
int arco_ugen_epoch(Ugen_id_ptr ugen_id);

/*SER void arco_ugen_free(int) PENT*/
void arco_ugen_free(int);

/*SER void arco_ugen_gc(Machine) PENT*/
void arco_ugen_gc(Machine_ptr m);

/*SER int arco_ugen_reset() PENT*/
int arco_ugen_reset();

/*SER void arco_ugen_gc_info(Machine) PENT*/
void arco_ugen_gc_info(Machine_ptr m);
