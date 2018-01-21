#ifndef __KNUTH_H__
#define __KNUTH_H__

#ifdef __cplusplus
extern "C" {
#endif

#define K_LIST_CLASSES 8

struct knuth
{
    uint32_t * buffer;
    size_t     buffer_bytes;

    // maintain categories of free lists
    uint32_t   power;
    uint32_t   lists[K_LIST_CLASSES];
    // allocation classes are in powers of 4:
    // 4, 16, 64, 128, 512, 2048, 8196 ...
};

/**
 * Initializes a Knuth state object
 * @param   state       Knuth state object
 * @param   buff        Buffer to allocate from
 * @param   buff_size   Buffer size in bytes
 * @param   power       Power of 2 for each allocation class
 */
void knuth_init(struct knuth * state, void * buff,
                size_t buff_size, uint32_t power);

void * knuth_malloc(struct knuth * state, size_t size);
void * knuth_calloc(struct knuth * state, size_t nmemb, size_t size);
void * knuth_realloc(struct knuth * state, void * ptr, size_t size);
void knuth_free(struct knuth * state, void * ptr);

#ifdef __cplusplus
}
#endif

#endif//__KNUTH_H__
