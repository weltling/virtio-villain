/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Selftests for the vring library.
 * Runs on the host - no VM needed. Tests struct sizes, layout,
 * and raw ring manipulation functions.
 */
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../lib/vring.h"
#include "../tests/test.h"

static int tests_run;
static int tests_passed;
static const char *c_pass = "";
static const char *c_fail = "";
static const char *c_reset = "";

#define CHECK(cond, fmt, ...) do { \
    tests_run++; \
    if (!(cond)) { \
        printf("  %s[FAIL]%s " fmt "\n", c_fail, c_reset, ##__VA_ARGS__); \
    } else { \
        printf("  %s[PASS]%s " fmt "\n", c_pass, c_reset, ##__VA_ARGS__); \
        tests_passed++; \
    } \
} while (0)

/* Dummy test function for registry testing */
static test_result_t dummy_test(struct virtio_dev *dev, struct vring *vr)
{
    (void)dev; (void)vr;
    return TEST_PASS;
}

REGISTER_TEST(selftest_dummy, 0x0002, dummy_test,
              "Dummy test for selftest registry", VIRTIO_SPEC_V1_2, "2.1");

/* --- Struct size / layout checks --- */

static void test_struct_sizes(void)
{
    CHECK(sizeof(struct vring_desc) == 16,
          "vring_desc size = %zu (want 16)", sizeof(struct vring_desc));
    CHECK(offsetof(struct vring_desc, addr) == 0,
          "vring_desc.addr offset = %zu (want 0)", offsetof(struct vring_desc, addr));
    CHECK(offsetof(struct vring_desc, len) == 8,
          "vring_desc.len offset = %zu (want 8)", offsetof(struct vring_desc, len));
    CHECK(offsetof(struct vring_desc, flags) == 12,
          "vring_desc.flags offset = %zu (want 12)", offsetof(struct vring_desc, flags));
    CHECK(offsetof(struct vring_desc, next) == 14,
          "vring_desc.next offset = %zu (want 14)", offsetof(struct vring_desc, next));

    CHECK(sizeof(struct vring_used_elem) == 8,
          "vring_used_elem size = %zu (want 8)", sizeof(struct vring_used_elem));

    CHECK(sizeof(struct test_entry) == 128,
          "test_entry size = %zu (want 128)", sizeof(struct test_entry));
}

/* --- Raw ring manipulation --- */

static void test_raw_set_desc(void)
{
    struct vring_desc desc[16];
    struct vring_avail avail;
    struct vring_used used;
    struct vring vr = {
        .desc = desc,
        .avail = &avail,
        .used = &used,
        .size = 16,
    };
    memset(desc, 0, sizeof(desc));

    vring_raw_set_desc(&vr, 3, 0xdeadbeef000ULL, 4096,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 7);

    CHECK(desc[3].addr == 0xdeadbeef000ULL,
          "desc[3].addr = 0x%llx (want 0xdeadbeef000)",
          (unsigned long long)desc[3].addr);
    CHECK(desc[3].len == 4096,
          "desc[3].len = %u (want 4096)", desc[3].len);
    CHECK(desc[3].flags == (VRING_DESC_F_NEXT | VRING_DESC_F_WRITE),
          "desc[3].flags = 0x%x (want 0x3)", desc[3].flags);
    CHECK(desc[3].next == 7,
          "desc[3].next = %u (want 7)", desc[3].next);

    /* Other slots untouched */
    CHECK(desc[0].addr == 0 && desc[0].len == 0,
          "desc[0] untouched");
}

static void test_raw_set_avail(void)
{
    struct {
        struct vring_avail hdr;
        uint16_t ring[16];
    } avail_buf;
    memset(&avail_buf, 0, sizeof(avail_buf));

    struct vring vr = {
        .avail = &avail_buf.hdr,
        .size = 16,
    };

    vring_raw_set_avail(&vr, 5, 42);
    CHECK(avail_buf.ring[5] == 42,
          "avail.ring[5] = %u (want 42)", avail_buf.ring[5]);

    vring_raw_set_avail_idx(&vr, 99);
    CHECK(avail_buf.hdr.idx == 99,
          "avail.idx = %u (want 99)", avail_buf.hdr.idx);
}

/* --- Test registry --- */

static void test_registry(void)
{
    int n = test_count();
    CHECK(n >= 1, "test_count() = %d (want >= 1)", n);

    struct test_entry *t = test_get(0);
    CHECK(t != NULL, "test_get(0) != NULL");
    CHECK(t->name != NULL && t->name[0] != '\0',
          "test_get(0)->name = \"%s\"", t ? t->name : "(null)");
    CHECK(t->fn != NULL, "test_get(0)->fn != NULL");

    /* Find by name */
    struct test_entry *found = test_find("selftest_dummy");
    CHECK(found != NULL, "test_find(\"selftest_dummy\") != NULL");
    CHECK(found != NULL && found->fn == dummy_test,
          "test_find(\"selftest_dummy\")->fn == dummy_test");

    /* Not found */
    struct test_entry *nope = test_find("NONEXISTENT_TEST_XYZ");
    CHECK(nope == NULL, "test_find(bogus) == NULL");
}

/* --- Main --- */

int main(void)
{
    const char *no_color = getenv("NO_COLOR");
    if (isatty(STDOUT_FILENO) && (no_color == NULL || no_color[0] == '\0')) {
        c_pass = "\033[32m";
        c_fail = "\033[31m";
        c_reset = "\033[0m";
    }
    printf("selftest/lib:\n");
    test_struct_sizes();
    test_raw_set_desc();
    test_raw_set_avail();
    test_registry();
    int ok = (tests_passed == tests_run);
    const char *tag = ok ? c_pass : c_fail;
    printf("\n%sselftest/lib: %d/%d passed%s\n",
           tag, tests_passed, tests_run, c_reset);
    return ok ? 0 : 1;
}
