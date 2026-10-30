/* SPDX-License-Identifier: Apache-2.0 */
#ifndef VV_GENERIC_TEST_H
#define VV_GENERIC_TEST_H

#include <stdio.h>

/*
 * Keep diagnostic prefixes distinct from final verdict records. Functions
 * using these return helpers must leave resource cleanup to their harness.
 */
#define TFAIL(fmt, ...) do { \
    printf("vv-fail %s:%d: " fmt "\n", __FILE__, \
           __LINE__ __VA_OPT__(,) __VA_ARGS__); \
    fflush(stdout); \
    return TEST_FAIL; \
} while (0)

#define TWEDGED(fmt, ...) do { \
    printf("vv-wedged %s:%d: " fmt "\n", __FILE__, \
           __LINE__ __VA_OPT__(,) __VA_ARGS__); \
    fflush(stdout); \
    return TEST_WEDGED; \
} while (0)

#define TREJECT(fmt, ...) do { \
    printf("vv-reject %s:%d: " fmt "\n", __FILE__, \
           __LINE__ __VA_OPT__(,) __VA_ARGS__); \
    fflush(stdout); \
    return TEST_REJECT; \
} while (0)

#define TSKIP(fmt, ...) do { \
    printf("vv-skip %s:%d: " fmt "\n", __FILE__, \
           __LINE__ __VA_OPT__(,) __VA_ARGS__); \
    fflush(stdout); \
    return TEST_SKIP; \
} while (0)

typedef enum {
    TEST_PASS,
    TEST_FAIL,
    TEST_SKIP,
    TEST_WEDGED,
    TEST_REJECT,
    TEST_XFAIL,
    TEST_XPASS
} test_result_t;

struct vv_test_entry {
    const char *name;
    const char *desc;
    test_result_t (*fn)(void *context);
};

#define VV_REGISTER_TEST(registry, name, function, description) \
    static test_result_t _vv_call_##name(void *context) { return function(context); } \
    __attribute__((section(#registry), used, aligned(sizeof(void *)))) \
    static const struct vv_test_entry _vv_test_##name = { \
        #name, description, _vv_call_##name \
    }

#endif
