#include <cstdint>
#include <cstddef>
#include <iostream>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

extern "C"
{
    void knuth_init(void * buff, size_t buff_size);
    void * knuth_malloc(size_t size);
    void * knuth_calloc(size_t nmemb, size_t size);
    void * knuth_realloc(void * ptr, size_t size);
    void knuth_free(void * ptr);
}


std::string print_buffer(int32_t * buffer, size_t num)
{
    std::stringstream sstream;
    static char buf[16];
    for (size_t i = 0; i < num; ++i) {
        sprintf(buf, "[%02d]", i);
        sstream << buf << ": " << buffer[i] << std::endl;
    }
    return sstream.str();
}
#define pbuf(buffer) print_buffer(buffer, sizeof(buffer)/sizeof(buffer[0]))

TEST(knuth, init)
{
    static int32_t buffer[128];
    knuth_init(buffer, sizeof(buffer));
    ASSERT_EQ(126, buffer[0]) << pbuf(buffer);
    ASSERT_EQ(126, buffer[127]) << pbuf(buffer);
}

TEST(knuth, malloc_small)
{
    static int32_t buffer[8];
    knuth_init(buffer, sizeof(buffer));
    void * p = knuth_malloc(1);
    ASSERT_EQ(-2, buffer[0]) << pbuf(buffer);
    ASSERT_EQ(-2, buffer[3]) << pbuf(buffer);
    ASSERT_EQ( 2, buffer[4]) << pbuf(buffer);
    ASSERT_EQ( 2, buffer[7]) << pbuf(buffer);
    knuth_free(p);
}

TEST(knuth, malloc_aligned)
{
    static int32_t buffer[32];
    knuth_init(buffer, sizeof(buffer));
    void * p = knuth_malloc(2 * 4);
    ASSERT_EQ(-2, buffer[0]) << pbuf(buffer);
    ASSERT_EQ(-2, buffer[3]) << pbuf(buffer);
    ASSERT_EQ(26, buffer[4]) << pbuf(buffer);
    ASSERT_EQ(26, buffer[31]) << pbuf(buffer);
    knuth_free(p);
}

TEST(knuth, malloc_unaligned)
{
    static int32_t buffer[32];
    knuth_init(buffer, sizeof(buffer));
    void * p = knuth_malloc(2 * 4 + 2);
    ASSERT_EQ(-3, buffer[0]) << pbuf(buffer);
    ASSERT_EQ(-3, buffer[4]) << pbuf(buffer);
    ASSERT_EQ(25, buffer[5]) << pbuf(buffer);
    ASSERT_EQ(25, buffer[31]) << pbuf(buffer);
    knuth_free(p);
}

TEST(knuth, calloc_aligned)
{
    static int32_t buffer[32];
    knuth_init(buffer, sizeof(buffer));
    void * p = knuth_calloc(sizeof(int32_t), 2);
    ASSERT_EQ(-2, buffer[0]) << pbuf(buffer);
    ASSERT_EQ( 0, buffer[1]) << pbuf(buffer);
    ASSERT_EQ( 0, buffer[2]) << pbuf(buffer);
    ASSERT_EQ(-2, buffer[3]) << pbuf(buffer);
    knuth_free(p);
}

TEST(knuth, calloc_unaligned)
{
    static int32_t buffer[32];
    knuth_init(buffer, sizeof(buffer));
    void * p = knuth_calloc(sizeof(uint8_t), 2 * 4 + 2);
    ASSERT_EQ(-3, buffer[0]) << pbuf(buffer);
    ASSERT_EQ( 0, buffer[1]) << pbuf(buffer);
    ASSERT_EQ( 0, buffer[2]) << pbuf(buffer);
    ASSERT_EQ( 0, buffer[3]) << pbuf(buffer);
    ASSERT_EQ(-3, buffer[4]) << pbuf(buffer);
    ASSERT_EQ(25, buffer[5]) << pbuf(buffer);
    ASSERT_EQ(25, buffer[31]) << pbuf(buffer);
    knuth_free(p);
}

TEST(knuth, free)
{
    static int32_t buffer[16];
    knuth_init(buffer, sizeof(buffer));
    void * p = knuth_malloc(2 * 4);
    ASSERT_EQ(-2, buffer[0]) << pbuf(buffer);
    ASSERT_EQ(-2, buffer[3]) << pbuf(buffer);
    ASSERT_EQ(10, buffer[4]) << pbuf(buffer);
    ASSERT_EQ(10, buffer[15]) << pbuf(buffer);
    knuth_free(p);
    // should coalesce these
    ASSERT_EQ(14, buffer[0]) << pbuf(buffer);
    ASSERT_EQ(14, buffer[15]) << pbuf(buffer);
}

TEST(knuth, free_coalesce)
{
    static int32_t buffer[20];
    knuth_init(buffer, sizeof(buffer));
    void * ptrs[5];
    for (int i = 0; i < 5; ++i) {
        ptrs[i] = knuth_malloc(sizeof(int32_t) * 2);
    }

    knuth_free(ptrs[0]);
    knuth_free(ptrs[4]);
    knuth_free(ptrs[1]);
    knuth_free(ptrs[3]);
    knuth_free(ptrs[2]);

    // should have coalesced into one big chunk
    ASSERT_EQ(18, buffer[0]) << pbuf(buffer);
    ASSERT_EQ(18, buffer[19]) << pbuf(buffer);
}
