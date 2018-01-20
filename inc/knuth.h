#ifndef __KNUTH_H__
#define __KNUTH_H__

#ifdef __cplusplus
extern "C" {
#endif

struct knuth
{
    uint32_t * buffer;
    size_t     buffer_bytes;
    uint32_t   base;
};

void knuth_init(struct knuth * state, void * buff, size_t buff_size);
void * knuth_malloc(struct knuth * state, size_t size);
void * knuth_calloc(struct knuth * state, size_t nmemb, size_t size);
void * knuth_realloc(struct knuth * state, void * ptr, size_t size);
void knuth_free(struct knuth * state, void * ptr);

#ifdef __cplusplus
}
#endif

#endif//__KNUTH_H__
