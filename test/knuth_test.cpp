#include <cstdint>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <set>

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

extern std::string print_free_list(struct knuth * state, int * ret);

TEST(knuth, init)
{
    struct knuth state;
    static int32_t buffer[128];
    knuth_init(&state, buffer, sizeof(buffer), 2);
    ASSERT_EQ(126, buffer[0]) << pbuf(buffer);
    ASSERT_EQ(126, buffer[127]) << pbuf(buffer);
}

TEST(knuth, malloc_small)
{
    struct knuth state;
    static int32_t buffer[8];
    knuth_init(&state, buffer, sizeof(buffer), 2);
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
    knuth_init(&state, buffer, sizeof(buffer), 2);
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
    knuth_init(&state, buffer, sizeof(buffer), 2);
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
    knuth_init(&state, buffer, sizeof(buffer), 2);
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
    knuth_init(&state, buffer, sizeof(buffer), 2);
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
    knuth_init(&state, buffer, sizeof(buffer), 2);
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
    knuth_init(&state, buffer, sizeof(buffer), 2);
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
    knuth_init(&state, buffer, sizeof(buffer), 2);
    const char * expect = "0123456789";
    char * str = (char *) knuth_malloc(&state, sizeof(char) * (strlen(expect) + 1));
    strcpy(str, expect);
    char * new_str = (char *) knuth_realloc(&state, str, sizeof(char) * (strlen(expect) + 2));
    ASSERT_EQ(str, new_str);
    ASSERT_STREQ(expect, new_str);
}

// coalesce to the right, in place
TEST(knuth, realloc_coalesce_r)
{
    struct knuth state;
    static int32_t buffer[20];
    knuth_init(&state, buffer, sizeof(buffer), 2);
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

    ASSERT_EQ(ptrs[2], new_str) << pbuf(buffer);
    ASSERT_STREQ(expect, new_str) << pbuf(buffer);
    ASSERT_EQ(-3, buffer[8]) << pbuf(buffer);
    ASSERT_EQ(-3, buffer[12]) << pbuf(buffer);
}

TEST(knuth, realloc_coalesce)
{
    struct knuth state;
    static int32_t buffer[20];
    knuth_init(&state, buffer, sizeof(buffer), 2);
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
    char * new_str = (char *) knuth_realloc(&state, ptrs[2], sizeof(int32_t) * 18);

    // coalesce should've placed this pointer where ptrs[0] is
    ASSERT_EQ(ptrs[0], new_str) << pbuf(buffer);
    ASSERT_STREQ(expect, new_str) << pbuf(buffer);
    ASSERT_EQ(-18, buffer[0]) << pbuf(buffer);
    ASSERT_EQ(-18, buffer[19]) << pbuf(buffer);
}

TEST(knuth, realloc_new)
{
    struct knuth state;
    static int32_t buffer[20];
    knuth_init(&state, buffer, sizeof(buffer), 2);
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

#define max(x,y) ((x > y) ? x : y)

TEST(knuth, many_allocs)
{
    struct knuth state;
    constexpr int NUM_WORDS = 1024 * 1024;
    constexpr int SIZE = 128;
    /*
    constexpr int NUM_WORDS = 128;
    constexpr int SIZE = 32;
    */
    static int32_t buffer[NUM_WORDS];
    knuth_init(&state, buffer, sizeof(buffer), 2);
    std::set<void *> ptrs;

    ASSERT_EQ(NUM_WORDS - 2, buffer[0]);
    ASSERT_EQ(NUM_WORDS - 2, buffer[NUM_WORDS - 1]);

    srand(0);
    void * ptr = NULL;
    size_t count = 0;
    do {
        ptr = knuth_malloc(&state, max(rand() % SIZE, 1));
        ptrs.insert(ptr);
        ++count;
    } while (ptr != NULL);

    for (void * ptr : ptrs) {
        knuth_free(&state, ptr);
        --count;
    }

    // check that we've completely freed all pointers
    // at this point, should have coalesced back into one big chunk
    ASSERT_EQ(0, count) << pbuf(buffer);
    ASSERT_EQ(NUM_WORDS - 2, buffer[0]) << pbuf(buffer);
    ASSERT_EQ(NUM_WORDS - 2, buffer[NUM_WORDS - 1]) << pbuf(buffer);
}

//#define PRINT_FREE_LIST

TEST(knuth, many_allocs_and_frees)
{
    struct knuth state;
    ///*
    constexpr int NUM_WORDS = 1024 * 1024;
    constexpr int SIZE = 4096;
    constexpr int ACTIONS = 1 << 16;
    //*/
    /*
    constexpr int NUM_WORDS = 128;
    constexpr int SIZE = 32;
    constexpr int ACTIONS = 128;
    */
    static int32_t buffer[NUM_WORDS];
    knuth_init(&state, buffer, sizeof(buffer), 2);
    std::set<void *> ptrs;

    ASSERT_EQ(NUM_WORDS - 2, buffer[0]);
    ASSERT_EQ(NUM_WORDS - 2, buffer[NUM_WORDS - 1]);

    srand(0);
    void * ptr = NULL;

    size_t count = 0;
    for (int i = 0; i < ACTIONS; ++i) {
        #ifdef PRINT_FREE_LIST
        std::cout << "i = " << i << std::endl;
        #endif
        int r = rand() % 2;
        if (r == 0 || ptrs.empty()) {
            // malloc random amount
            ptr = knuth_malloc(&state, max(rand() % SIZE, 1));
            if (ptr != NULL) {
                ptrs.insert(ptr);
                ++count;
                #ifdef PRINT_FREE_LIST
                std::cout << "Action: malloc" << std::endl;
                #endif
            }
        } else {
            // free random ptr
            std::set<void *>::iterator it = ptrs.begin();
            std::advance(it, rand() % ptrs.size());
            ptr = *it;
            ptrs.erase(ptr);
            knuth_free(&state, ptr);
            --count;
            #ifdef PRINT_FREE_LIST
            std::cout << "Action: free" << std::endl;
            #endif
        }
        #ifdef PRINT_FREE_LIST
        int ret = 1;
        std::cout << print_free_list(&state, &ret) << std::endl;
        ASSERT_EQ(1, ret);
        #endif
    }

    for (void * ptr : ptrs) {
        knuth_free(&state, ptr);
        --count;
    }
    ptrs.clear();

    // check that we've completely freed all pointers
    // at this point, should have coalesced back into one big chunk
    ASSERT_EQ(0, count) << pbuf(buffer);
    ASSERT_EQ(NUM_WORDS - 2, buffer[0]) << pbuf(buffer);
    ASSERT_EQ(NUM_WORDS - 2, buffer[NUM_WORDS - 1]) << pbuf(buffer);
}

TEST(knuth, many_reallocs)
{
    struct knuth state;
    constexpr int NUM_WORDS = 1024 * 1024;
    static int32_t buffer[NUM_WORDS];
    knuth_init(&state, buffer, sizeof(buffer), 2);

    const char * expect = "hello";

    char * str = (char *) knuth_malloc(&state, 6);
    strcpy(str, expect);
    ASSERT_STREQ(expect, str);

    for (int i = 0; i < (NUM_WORDS-10) / 10; ++i) {
        str = (char *) knuth_realloc(&state, str, 10 * (i + 1));
        ASSERT_STREQ(expect, str) << "Failed at iteration " << i;
    }

    knuth_free(&state, str);

    ASSERT_EQ(NUM_WORDS - 2, buffer[0]) << pbuf(buffer);
    ASSERT_EQ(NUM_WORDS - 2, buffer[NUM_WORDS - 1]) << pbuf(buffer);
}
