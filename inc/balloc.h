#ifndef __BALLOC_H__
#define __BALLOC_H__

#ifdef __cplusplus
extern "C" {
#endif

#define BALLOC_LIST_CLASSES 8

struct balloc
{
    uint32_t * buffer;
    size_t     buffer_bytes;

    // maintain categories of free lists
    uint32_t   power;
    uint32_t   lists[BALLOC_LIST_CLASSES];
    // allocation classes are in powers of 4:
    // 4, 16, 64, 128, 512, 2048, 8196 ...
};

/**
 * Initializes a balloc state object
 * @param   state       balloc state object
 * @param   buff        Buffer to allocate from
 * @param   buff_size   Buffer size in bytes
 * @param   power       Power of 2 for each allocation class
 */
void balloc_init(struct balloc * state, void * buff,
                size_t buff_size, uint32_t power);

void * balloc_malloc(struct balloc * state, size_t size);
void * balloc_calloc(struct balloc * state, size_t nmemb, size_t size);
void * balloc_realloc(struct balloc * state, void * ptr, size_t size);
void balloc_free(struct balloc * state, void * ptr);

#ifdef __cplusplus
}
#endif

#endif//__BALLOC_H__
