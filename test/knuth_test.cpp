#include <cstdint>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include <knuth.h>

std::string print_buffer(int32_t * buffer, size_t num)
{
    std::stringstream sstream;
    static char buf[64];
    for (size_t i = 0; i < num; ++i) {
        sprintf(buf, "[%02d]: 0x%08X | %d", i, buffer[i], buffer[i]);
        sstream << buf << std::endl;
    }
    return sstream.str();
}
#define pbuf(buffer) print_buffer(buffer, sizeof(buffer)/sizeof(buffer[0]))

TEST(knuth, init)
{
    struct knuth state;
    static int32_t buffer[128];
    knuth_init(&state, buffer, sizeof(buffer));
    ASSERT_EQ(126, buffer[0]) << pbuf(buffer);
    ASSERT_EQ(126, buffer[127]) << pbuf(buffer);
}

TEST(knuth, malloc_small)
{
    struct knuth state;
    static int32_t buffer[8];
    knuth_init(&state, buffer, sizeof(buffer));
    void * p = knuth_malloc(&state, 1);
    ASSERT_EQ(-2, buffer[0]) << pbuf(buffer);
    ASSERT_EQ(-2, buffer[3]) << pbuf(buffer);
    ASSERT_EQ( 2, buffer[4]) << pbuf(buffer);
    ASSERT_EQ( 2, buffer[7]) << pbuf(buffer);
    knuth_free(&state, p);
}

TEST(knuth, malloc_aligned)
{
    struct knuth state;
    static int32_t buffer[32];
    knuth_init(&state, buffer, sizeof(buffer));
    void * p = knuth_malloc(&state, 2 * 4);
    ASSERT_EQ(-2, buffer[0]) << pbuf(buffer);
    ASSERT_EQ(-2, buffer[3]) << pbuf(buffer);
    ASSERT_EQ(26, buffer[4]) << pbuf(buffer);
    ASSERT_EQ(26, buffer[31]) << pbuf(buffer);
    knuth_free(&state, p);
}

TEST(knuth, malloc_unaligned)
{
    struct knuth state;
    static int32_t buffer[32];
    knuth_init(&state, buffer, sizeof(buffer));
    void * p = knuth_malloc(&state, 2 * 4 + 2);
    ASSERT_EQ(-3, buffer[0]) << pbuf(buffer);
    ASSERT_EQ(-3, buffer[4]) << pbuf(buffer);
    ASSERT_EQ(25, buffer[5]) << pbuf(buffer);
    ASSERT_EQ(25, buffer[31]) << pbuf(buffer);
    knuth_free(&state, p);
}

TEST(knuth, calloc_aligned)
{
    struct knuth state;
    static int32_t buffer[32];
    knuth_init(&state, buffer, sizeof(buffer));
    void * p = knuth_calloc(&state, sizeof(int32_t), 2);
    ASSERT_EQ(-2, buffer[0]) << pbuf(buffer);
    ASSERT_EQ( 0, buffer[1]) << pbuf(buffer);
    ASSERT_EQ( 0, buffer[2]) << pbuf(buffer);
    ASSERT_EQ(-2, buffer[3]) << pbuf(buffer);
    knuth_free(&state, p);
}

TEST(knuth, calloc_unaligned)
{
    struct knuth state;
    static int32_t buffer[32];
    knuth_init(&state, buffer, sizeof(buffer));
    void * p = knuth_calloc(&state, sizeof(uint8_t), 2 * 4 + 2);
    ASSERT_EQ(-3, buffer[0]) << pbuf(buffer);
    ASSERT_EQ( 0, buffer[1]) << pbuf(buffer);
    ASSERT_EQ( 0, buffer[2]) << pbuf(buffer);
    ASSERT_EQ( 0, buffer[3]) << pbuf(buffer);
    ASSERT_EQ(-3, buffer[4]) << pbuf(buffer);
    ASSERT_EQ(25, buffer[5]) << pbuf(buffer);
    ASSERT_EQ(25, buffer[31]) << pbuf(buffer);
    knuth_free(&state, p);
}

TEST(knuth, free)
{
    struct knuth state;
    static int32_t buffer[16];
    knuth_init(&state, buffer, sizeof(buffer));
    void * p = knuth_malloc(&state, 2 * 4);
    ASSERT_EQ(-2, buffer[0]) << pbuf(buffer);
    ASSERT_EQ(-2, buffer[3]) << pbuf(buffer);
    ASSERT_EQ(10, buffer[4]) << pbuf(buffer);
    ASSERT_EQ(10, buffer[15]) << pbuf(buffer);
    knuth_free(&state, p);
    // should coalesce these
    ASSERT_EQ(14, buffer[0]) << pbuf(buffer);
    ASSERT_EQ(14, buffer[15]) << pbuf(buffer);
}

TEST(knuth, free_coalesce)
{
    struct knuth state;
    static int32_t buffer[20];
    knuth_init(&state, buffer, sizeof(buffer));
    void * ptrs[5];
    for (int i = 0; i < 5; ++i) {
        ptrs[i] = knuth_malloc(&state, sizeof(int32_t) * 2);
    }

    knuth_free(&state, ptrs[0]);
    knuth_free(&state, ptrs[4]);
    knuth_free(&state, ptrs[1]);
    knuth_free(&state, ptrs[3]);
    knuth_free(&state, ptrs[2]);

    // should have coalesced into one big chunk
    ASSERT_EQ(18, buffer[0]) << pbuf(buffer);
    ASSERT_EQ(18, buffer[19]) << pbuf(buffer);
}

TEST(knuth, realloc_same)
{
    struct knuth state;
    static int32_t buffer[8];
    knuth_init(&state, buffer, sizeof(buffer));
    const char * expect = "0123456789";
    char * str = (char *) knuth_malloc(&state, sizeof(char) * (strlen(expect) + 1));
    strcpy(str, expect);
    char * new_str = (char *) knuth_realloc(&state, str, sizeof(char) * (strlen(expect) + 2));
    ASSERT_EQ(str, new_str);
    ASSERT_STREQ(expect, new_str);
}

TEST(knuth, realloc_coalesce)
{
    struct knuth state;
    static int32_t buffer[20];
    knuth_init(&state, buffer, sizeof(buffer));
    void * ptrs[5];
    for (int i = 0; i < 5; ++i) {
        ptrs[i] = knuth_malloc(&state, sizeof(int32_t) * 2);
    }

    const char * expect = "hello";
    strcpy((char *)ptrs[2], expect);
    knuth_free(&state, ptrs[0]);
    knuth_free(&state, ptrs[4]);
    knuth_free(&state, ptrs[1]);
    knuth_free(&state, ptrs[3]);
    char * new_str = (char *) knuth_realloc(&state, ptrs[2], 12);

    // coalesce should've placed this pointer where ptrs[0] is
    ASSERT_EQ(ptrs[0], new_str) << pbuf(buffer);
    ASSERT_STREQ(expect, new_str) << pbuf(buffer);
    ASSERT_EQ(-3, buffer[0]) << pbuf(buffer);
    ASSERT_EQ(-3, buffer[4]) << pbuf(buffer);
}

TEST(knuth, realloc_new)
{
    struct knuth state;
    static int32_t buffer[20];
    knuth_init(&state, buffer, sizeof(buffer));
    void * ptrs[5];
    for (int i = 0; i < 5; ++i) {
        ptrs[i] = knuth_malloc(&state, sizeof(int32_t) * 2);
    }

    const char * expect = "hello";
    strcpy((char *)ptrs[4], expect);
    knuth_free(&state, ptrs[0]);
    knuth_free(&state, ptrs[1]);
    knuth_free(&state, ptrs[2]);
    char * new_str = (char *) knuth_realloc(&state, ptrs[4], 12);

    // coalesce should've placed this pointer where ptrs[0] is
    ASSERT_EQ(ptrs[0], new_str) << pbuf(buffer);
    ASSERT_STREQ(expect, new_str) << pbuf(buffer);
    ASSERT_EQ(-3, buffer[0]) << pbuf(buffer);
    ASSERT_EQ(-3, buffer[4]) << pbuf(buffer);
}
