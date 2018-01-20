#include <stdint.h>
#include <stddef.h>

/**
 * Knuth heap allocator with links
 * Optimal use of this allocator is for things larger than 8 bytes. The larger,
 * the better
 *
 * Aligned on a 4-byte boundary:
 * Each allocation requires a minimum of 16 bytes: 4 for header, 4 for footer,
 * 4 for forward link, 4 for backward link.
 *
 * The size of a chunk is stored in the header and footer for sanity
 * checking. When the size is negative, the chunk is taken.
 *
 * A link is an 4-byte offset (indexing as uint32) into the buffer that
 * corresponds to the location of the header. The links are used to maintain
 * a free list of chunks
 *
 * For a request that satisfies an N-byte request (N >= 16 && (N % 4 == 0)
 * -4   |=====================================
 *      | Header: Chunk size
 * +0   |------------------------------------- <-- chunk data starts
 *      | Forward link
 * +4   |-------------------------------------
 *      | Backward link
 * +8   |-------------------------------------
 *      | ...
 * +N   |------------------------------------- <-- chunk data ends
 *      | Footer: Must match header
 * +N+4 |=====================================
 */

uint32_t * buffer = NULL;
size_t     buffer_bytes = 0;
#define NIL 0xFFFFFFFF
uint32_t base = NIL;

struct chunk
{
    int32_t size;   // header
    uint32_t next;  // fwd link
    uint32_t prev;  // bck link
};

static inline
uint32_t round_up(size_t byte_size)
{
    uint32_t size = (byte_size + (byte_size & 0x3)) >> 2;
    return size;
}

static inline
uint32_t round_down(size_t byte_size)
{
    uint32_t size = byte_size >> 2;
    return size;
}

static inline
size_t to_bytes(uint32_t size)
{
    return (size << 2);
}

// get the offset in the buffer of a chunk
static inline
uint32_t get_offset(struct chunk * chunk)
{
    if (chunk == NULL)
        return NIL;

    uintptr_t byte_off = (uintptr_t) chunk - (uintptr_t) buffer;
    uint32_t off = round_down(byte_off);
    return off;
}

// offset into the buffer to get the chunk pointer
static inline
struct chunk * get_chunk(uint32_t offset)
{
    if (offset == NIL)
        return NULL;
    struct chunk * out = (struct chunk *) &buffer[offset];
    return out;
}

// returns pointer to data for a chunk
static inline
void * get_ptr(struct chunk * chunk)
{
    return &(chunk->next);
}

// returns chunk pointer from a pointer to the data
static inline
struct chunk * from_ptr(void * p)
{
    struct chunk * chunk = (struct chunk *) ((int32_t *) p - 1);
    return chunk;
}

#define abs(x) ((int32_t)(x) < 0 ? -(int32_t)(x) :  (int32_t)(x))
#define neg(x) ((int32_t)(x) < 0 ?  (int32_t)(x) : -(int32_t)(x))

// get size (4-byte words) of a chunk
static inline
uint32_t chunk_size(struct chunk * chunk)
{
    uint32_t size = abs(chunk->size) + 2;
    return size;
}

// gets pointer of the footer
static inline
int32_t * get_footer(struct chunk * chunk)
{
    int32_t size = abs(chunk->size);
    int32_t * foot = (int32_t *) &(chunk->next) + size;  // hooray for ptr arith
    return foot;
}

// returns true if header matches footer
static inline
int check_meta(struct chunk * chunk)
{
    return (chunk->size == *get_footer(chunk));
}

// sets the footer
static inline
void set_footer(struct chunk * chunk, int32_t footer_val)
{
    int32_t * footer = get_footer(chunk);
    *footer = footer_val;
}

static inline
struct chunk * from_footer(int32_t * foot)
{
    int32_t size = abs(*foot);
    struct chunk * chunk = (struct chunk *) (foot - size - 1);
    return chunk;
}

// traversal: follow next
static inline
struct chunk * get_next(struct chunk * chunk)
{
    if (chunk->next == NIL)
        return NULL;
    return get_chunk(chunk->next);
}

// traversal: follow prev
static inline
struct chunk * get_prev(struct chunk * chunk)
{
    if (chunk->prev == NIL)
        return NULL;
    return get_chunk(chunk->prev);
}

// traversal: get adjacent chunk after this one
static inline
struct chunk * get_adj_next(struct chunk * chunk)
{
    struct chunk * adj = (struct chunk *) (get_footer(chunk) + 1);

    // boundary check
    if (adj >= (((uint8_t *) buffer) + buffer_bytes))
        return NULL;
    return adj;
}

// traversal: get adjacent chunk before this one
static inline
struct chunk * get_adj_prev(struct chunk * chunk)
{
    int32_t * prev_foot = (int32_t *) (&(chunk->size) - 1);
    struct chunk * adj = from_footer(prev_foot);
    if (adj < buffer)
        return NULL;
    return adj;
}

// find the best chunk for this
static
struct chunk * find_best_chunk(size_t byte_size)
{
    if (base == NIL)
        return NULL;

    // round up the size
    int32_t size = round_up(byte_size);

    struct chunk * curr = get_chunk(base);
    while (curr != NULL) {
        if (curr->size >= size) {
            return curr;
        }
        curr = get_next(curr);
    }
    return curr;
}

// remove chunk from the list
static
void remove_free_chunk(struct chunk * chunk)
{
    struct chunk * prev = get_prev(chunk);
    struct chunk * next = get_next(chunk);
    if (prev == NULL && next == NULL) {
        // if there's nothing, base is now NIL
        base = NIL;
        return;
    } else if (prev == NULL) {
        // if prev is null, then next will be new base
        base = chunk->next;
        next->prev = NIL;
    } else if (next == NULL) {
        // if next is null, then pass the NIL to prev
        prev->next = NIL;
    } else {
        prev->next = chunk->next;
        next->prev = chunk->prev;
    }
}

// adds chunk to free list: smallest to largest
static
void add_free_chunk(struct chunk * chunk)
{
    if (base == NIL) {
        base = get_offset(chunk);
        chunk->next = NIL;
        chunk->prev = NIL;
        return;
    }

    struct chunk * curr = get_chunk(base);
    struct chunk * prev = NULL;
    while (curr != NULL) {
        // if it's larger than current, it goes there
        if (chunk->size > curr->size) {
            if (prev == NULL) {
                // new base
                base = get_offset(chunk);
                curr->prev = base;
                chunk->next = get_offset(curr);
                return;
            } else {
                // add after curr: move curr to next so we can use prev and curr
                prev = curr;
                curr = get_next(curr);

                prev->next = get_offset(chunk);
                curr->prev = get_offset(chunk);
                chunk->next = get_offset(curr);
                chunk->prev = get_offset(prev);
                return;
            }
        }

        prev = curr;
        curr = get_next(curr);
    }

    // if we get here, then this goes at the end
    prev->next  = get_offset(chunk);
    chunk->prev = get_offset(prev);
}

// applies the allocation to this chunk
// may break the the chunk up
static
void * allocate_chunk(struct chunk * chunk, size_t byte_size, int clear)
{
    // take this chunk out of the free list
    remove_free_chunk(chunk);

    // determine if chunk should be broken
    // break if we can fit another allocation chunk in all of this
    if (to_bytes(chunk->size + 2) >=
        sizeof(struct chunk) + sizeof(uint32_t) + byte_size) {
        // break
        uint32_t available_space = chunk_size(chunk) - 4;
        uint32_t size = round_up(byte_size);
        if (size < 2)
            size = 2;

        // resize the original chunk
        chunk->size = size;
        set_footer(chunk, chunk->size);

        // setup the new chunk
        struct chunk * new_chunk = get_adj_next(chunk);
        new_chunk->size = available_space - size;
        set_footer(new_chunk, new_chunk->size);
        add_free_chunk(new_chunk);
    } else {
        // don't break
        chunk->next = NIL;
        chunk->prev = NIL;
    }

    if (clear) {
        size_t words = round_up(byte_size);
        uint32_t * word = &(chunk->next);
        while (words-- > 0) {
            *word = 0x00000000;
            ++word;
        }
    }

    chunk->size = neg(chunk->size);
    set_footer(chunk, chunk->size);
    return get_ptr(chunk);
}

static
void * alloc(size_t n, int clear)
{
    if (n == 0)
        return NULL;
    struct chunk * chunk = find_best_chunk(n);
    if (chunk == NULL)
        return NULL;
    return allocate_chunk(chunk, n, clear);
}

// joins to chunks together and return a new chunk
// assumes that l and r have been removed from the free list
// l must have a lower address than r
static
struct chunk * join(struct chunk * l, struct chunk * r)
{
    // combine sizes, +2 for reclaiming a header and footer
    int32_t size = l->size + r->size + 2;
    l->size = size;
    set_footer(l, size);
    return l;
}

// coalesce a chunk with surrounding chunks, adding it to the free list
static
void coalesce(struct chunk * chunk)
{
    // coalesce right
    struct chunk * r = get_adj_next(chunk);
    while (r != NULL && r->size > 0) {
        // if free, remove the adjacent chunk from the list and join it
        remove_free_chunk(r);
        chunk = join(chunk, r);
        r = get_adj_next(chunk);
    }

    // coalesce left
    struct chunk * l = get_adj_prev(chunk);
    while (l != NULL && l->size > 0) {
        // if free, remove the adjacent chunk from the list and join it
        remove_free_chunk(l);
        chunk = join(l, chunk);
        l = get_adj_prev(chunk);
    }

    // add the final chunk to the free list
    add_free_chunk(chunk);
}

void knuth_init(void * buff, size_t buff_size)
{
    buffer = (int32_t *) buff;
    buffer_bytes = buff_size;

    // setup the initial chunk
    struct chunk * init = (struct chunk *) buffer;
    init->size = (int32_t) round_up(buffer_bytes) - 2;
    init->next = NIL;
    init->prev = NIL;
    set_footer(init, init->size);

    // add it to the free list
    base = get_offset(init);
}

void * knuth_malloc(size_t size)
{
    return alloc(size, 0);
}

void * knuth_realloc(void * ptr, size_t size)
{
    return ptr;
}

void * knuth_calloc(size_t nmemb, size_t size)
{
    return alloc(nmemb * size, 1);
}

void knuth_free(void * ptr)
{
    if (ptr == NULL)
        return;

    // do a metadata check
    struct chunk * chunk = from_ptr(ptr);
    if (!check_meta(chunk)) {
        return;
        // ... or do some form of assertion fail
    }

    if (chunk->size >= 0) {
        return;
    }

    // see if we can coalesce this chunk with surrounding chunks
    chunk->size = abs(chunk->size);
    set_footer(chunk, chunk->size);
    coalesce(chunk);
}
