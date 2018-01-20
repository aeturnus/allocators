#include <stdint.h>
#include <stddef.h>
#include <knuth.h>

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
 * A link is an index into the uint32_t buffer that
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

#define NIL 0xFFFFFFFF

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
uint32_t get_offset(struct knuth * state, struct chunk * chunk)
{
    if (chunk == NULL)
        return NIL;

    uintptr_t byte_off = (uintptr_t) chunk - (uintptr_t) state->buffer;
    uint32_t off = round_down(byte_off);
    return off;
}

// offset into the buffer to get the chunk pointer
static inline
struct chunk * get_chunk(struct knuth * state, uint32_t offset)
{
    if (offset == NIL)
        return NULL;
    struct chunk * out = (struct chunk *) &state->buffer[offset];
    return out;
}

// returns pointer to data for a chunk
// if chunk is NULL, then return NULL
static inline
void * get_ptr(struct chunk * chunk)
{
    if (chunk == NULL)
        return NULL;
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

// get total space (4-byte words) taken by a chunk (includes metadata)
static inline
uint32_t chunk_space(struct chunk * chunk)
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
void set_size(struct chunk * chunk, int32_t size)
{
    chunk->size = size;
    set_footer(chunk, size);
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
struct chunk * get_next(struct knuth * state, struct chunk * chunk)
{
    if (chunk->next == NIL)
        return NULL;
    return get_chunk(state, chunk->next);
}

// traversal: follow prev
static inline
struct chunk * get_prev(struct knuth * state, struct chunk * chunk)
{
    if (chunk->prev == NIL)
        return NULL;
    return get_chunk(state, chunk->prev);
}

// traversal: get adjacent chunk after this one
static inline
struct chunk * get_adj_next(struct knuth * state, struct chunk * chunk)
{
    struct chunk * adj = (struct chunk *) (get_footer(chunk) + 1);

    // boundary check
    if ((void *) adj >=
        (void *) (((uint8_t *) state->buffer) + state->buffer_bytes))
        return NULL;
    return adj;
}

// traversal: get adjacent chunk before this one
static inline
struct chunk * get_adj_prev(struct knuth * state, struct chunk * chunk)
{
    int32_t * prev_foot = (int32_t *) (&(chunk->size) - 1);
    struct chunk * adj = from_footer(prev_foot);
    if ((void *) adj < (void *) state->buffer)
        return NULL;
    return adj;
}

// find the best chunk for this
static
struct chunk * find_best_chunk(struct knuth * state, size_t byte_size)
{
    if (state->base == NIL)
        return NULL;

    // round up the size
    int32_t size = round_up(byte_size);

    struct chunk * curr = get_chunk(state, state->base);
    while (curr != NULL) {
        if (curr->size >= size) {
            return curr;
        }
        curr = get_next(state, curr);
    }
    return curr;
}

// remove chunk from the list
static
void remove_free_chunk(struct knuth * state, struct chunk * chunk)
{
    struct chunk * prev = get_prev(state, chunk);
    struct chunk * next = get_next(state, chunk);
    if (prev == NULL && next == NULL) {
        // if there's nothing, base is now NIL
        state->base = NIL;
        return;
    } else if (prev == NULL) {
        // if prev is null, then next will be new base
        state->base = chunk->next;
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
void add_free_chunk(struct knuth * state, struct chunk * chunk)
{
    if (state->base == NIL) {
        state->base = get_offset(state, chunk);
        chunk->next = NIL;
        chunk->prev = NIL;
        return;
    }

    struct chunk * curr = get_chunk(state, state->base);
    struct chunk * prev = NULL;
    while (curr != NULL) {
        // if it's larger than current, it goes there
        if (chunk->size > curr->size) {
            if (prev == NULL) {
                // new base
                state->base = get_offset(state, chunk);
                curr->prev = state->base;
                chunk->next = get_offset(state, curr);
                return;
            } else {
                // add after curr: move curr to next so we can use prev and curr
                prev = curr;
                curr = get_next(state, curr);

                prev->next = get_offset(state, chunk);
                curr->prev = get_offset(state, chunk);
                chunk->next = get_offset(state, curr);
                chunk->prev = get_offset(state, prev);
                return;
            }
        }

        prev = curr;
        curr = get_next(state, curr);
    }

    // if we get here, then this goes at the end
    prev->next  = get_offset(state, chunk);
    chunk->prev = get_offset(state, prev);
}

// applies the allocation to this chunk
// may break the the chunk up
static
struct chunk * allocate_chunk(struct knuth * state, struct chunk * chunk, size_t byte_size, int clear)
{
    // take this chunk out of the free list
    remove_free_chunk(state, chunk);

    // determine if chunk should be broken
    // break if we can fit another allocation chunk in all of this
    if (to_bytes(chunk->size + 2) >=
        sizeof(struct chunk) + sizeof(uint32_t) + byte_size) {
        // break
        uint32_t available_space = chunk_space(chunk) - 4;
        uint32_t size = round_up(byte_size);
        if (size < 2)
            size = 2;

        // resize the original chunk
        set_size(chunk, size);

        // setup the new chunk
        struct chunk * new_chunk = get_adj_next(state, chunk);
        set_size(new_chunk, available_space - size);
        add_free_chunk(state, new_chunk);
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

    set_size(chunk, neg(chunk->size));
    return chunk;
}

// finds best chunk and allocates it
// returns the chunk
static
struct chunk * allocate(struct knuth * state, size_t n, int clear)
{
    if (n == 0)
        return NULL;
    struct chunk * chunk = find_best_chunk(state, n);
    if (chunk == NULL)
        return NULL;
    return allocate_chunk(state, chunk, n, clear);
}

// joins to chunks together and return a new chunk
// assumes that l and r have been removed from the free list
// l must have a lower address than r
static
struct chunk * join(struct chunk * l, struct chunk * r)
{
    // combine sizes, +2 for reclaiming a header and footer
    int32_t size = l->size + r->size + 2;
    set_size(l, size);
    return l;
}

// coalesce a chunk with surrounding chunks
// returns pointer to newly formed chunk
static
struct chunk * coalesce(struct knuth * state, struct chunk * chunk)
{
    // coalesce right
    struct chunk * r = get_adj_next(state, chunk);
    while (r != NULL && r->size > 0) {
        // if free, remove the adjacent chunk from the list and join it
        remove_free_chunk(state, r);
        chunk = join(chunk, r);
        r = get_adj_next(state, chunk);
    }

    // coalesce left
    struct chunk * l = get_adj_prev(state, chunk);
    while (l != NULL && l->size > 0) {
        // if free, remove the adjacent chunk from the list and join it
        remove_free_chunk(state, l);
        chunk = join(l, chunk);
        l = get_adj_prev(state, chunk);
    }

    return chunk;
}

// returns the potential size if a coalesce happens with a chunk
static
uint32_t coalesce_probe(struct knuth * state, struct chunk * chunk)
{
    // measure the total space
    uint32_t space = chunk_space(chunk);

    // right
    struct chunk * r = get_adj_next(state, chunk);
    while (r != NULL && r->size > 0) {
        space += chunk_space(r);
        r = get_adj_next(state, r);
    }

    // left
    struct chunk * l = get_adj_prev(state, chunk);
    while (l != NULL && l->size > 0) {
        space += chunk_space(l);
        l = get_adj_prev(state, l);
    }

    return space;
}

// transfer n uint32_t's from src to dst
// guaranteed to not destroy data
static
void transfer(uint32_t * dst, uint32_t * src, int32_t n)
{
    if (src < dst) {
        // start from beginning
        for (int32_t i = 0; i < n; ++i) {
            dst[i] = src[i];
        }
    } else if (src > dst) {
        // start from end
        for (int32_t i = n-1; i >= 0 ; --i) {
            dst[i] = src[i];
        }
    }
}

// deallocates a chunk, coalescing it if possible
static
void deallocate(struct knuth * state, struct chunk * chunk)
{
    // see if we can coalesce this chunk with surrounding chunks
    set_size(chunk, abs(chunk->size));
    chunk = coalesce(state, chunk);
    add_free_chunk(state, chunk);
}

static
struct chunk * reallocate(struct knuth * state, struct chunk * chunk, size_t byte_size)
{
    uint32_t size = round_up(byte_size);
    // 3 cases
    // 1: chunk is already of requested size
    // 2: we can coalesce around this chunk
    // 3: otherwise, we need to find a new chunk

    // case 1
    if ((uint32_t) abs(chunk->size) >= size) {
        return chunk;
    }

    // save number of words to copy and buffer location
    uint32_t * src = (uint32_t *) get_ptr(chunk);
    int32_t num_words = abs(chunk->size);
    uint32_t * dst = NULL;

    // case 2
    if (coalesce_probe(state, chunk) - 2 >= size) {
        // perform the coalesce, then transfer the data
        // coalesce() only messes with headers and footers: safe for transfer
        // allocate_chunk() can break up the chunk
        // since join() assumes that chunks are free, make the size positive
        set_size(chunk, abs(chunk->size));
        chunk = coalesce(state, chunk);
        chunk = allocate_chunk(state, chunk, byte_size, 0);
        dst = (uint32_t *) get_ptr(chunk);
        transfer(dst, src, num_words);
        return chunk;
    }

    // case 3
    struct chunk * new_chunk = allocate(state, byte_size, 0);
    if (new_chunk == NULL)
        return NULL;
    dst = (uint32_t *) get_ptr(new_chunk);
    transfer(dst, src, num_words);
    deallocate(state, chunk);

    return new_chunk;
}

void knuth_init(struct knuth * state, void * buff, size_t buff_size)
{
    state->buffer = (uint32_t *) buff;
    state->buffer_bytes = buff_size;

    // setup the initial chunk
    struct chunk * init = (struct chunk *) state->buffer;
    init->size = (int32_t) round_up(state->buffer_bytes) - 2;
    init->next = NIL;
    init->prev = NIL;
    set_footer(init, init->size);

    // add it to the free list
    state->base = get_offset(state, init);
}

void * knuth_malloc(struct knuth * state, size_t size)
{
    return get_ptr(allocate(state, size, 0));
}

void * knuth_calloc(struct knuth * state, size_t nmemb, size_t size)
{
    return get_ptr(allocate(state, nmemb * size, 1));
}

void knuth_free(struct knuth * state, void * ptr)
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
        // ... or do some form of assertion fail
    }

    deallocate(state, chunk);
}

void * knuth_realloc(struct knuth * state, void * ptr, size_t size)
{
    // defined: realloc with NULL ptr is a malloc
    if (ptr == NULL)
        return knuth_malloc(state, size);

    // defined: realloc with size 0 is a free
    if (size == 0) {
        knuth_free(state, ptr);
        return NULL;
    }

    // do a metadata check
    struct chunk * chunk = from_ptr(ptr);
    if (!check_meta(chunk)) {
        return NULL;
        // ... or do some form of assertion fail
    }

    if (chunk->size >= 0) {
        return NULL;
        // ... or do some form of assertion fail
    }

    return get_ptr(reallocate(state, chunk, size));
}
