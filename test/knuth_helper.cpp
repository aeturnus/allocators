#include <cstdint>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

#include <knuth.h>

// copied from knuth.c for debugging purposes
static inline
uint32_t round_down(size_t byte_size)
{
    uint32_t size = byte_size >> 2;
    return size;
}

#define NIL 0xFFFFFFFF
struct chunk
{
    int32_t size;   // header
    uint32_t next;  // fwd link
    uint32_t prev;  // bck link
};

static inline
struct chunk * get_chunk(struct knuth * state, uint32_t offset)
{
    if (offset == NIL)
        return NULL;
    struct chunk * out = (struct chunk *) &state->buffer[offset];
    return out;
}

static inline
uint32_t get_offset(struct knuth * state, struct chunk * chunk)
{
    if (chunk == NULL)
        return NIL;

    uintptr_t byte_off = (uintptr_t) chunk - (uintptr_t) state->buffer;
    uint32_t off = round_down(byte_off);
    return off;
}

std::string print_free_list(struct knuth * state, int * ret)
{
    std::stringstream sstream;
    static char buf[128];

    struct chunk * curr = get_chunk(state, state->base);
    struct chunk * prev = NULL;
    sprintf(buf, "Knuth free list::\n");
    sstream << buf;
    while (curr != NULL) {
        if (curr->size < 0) {
            sprintf(buf, "%d: size = %d : ERROR\n\n", get_offset(state, curr), curr->size);
            sstream << buf;
            *ret = 0;
            return sstream.str();
        }
        sprintf(buf, "%d: size = %d\n", get_offset(state, curr), curr->size);
        sstream << buf;
        prev = curr;
        curr = get_chunk(state, curr->next);
        if (curr == prev) {
            sprintf(buf, "%d: size = %d : ERROR - cycle detected", get_offset(state, curr), curr->size);
            sstream << buf;
        }
    }
    sprintf(buf, "\n");
    sstream << buf;
    *ret = 1;
    return sstream.str();
}
