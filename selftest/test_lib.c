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

#include "../lib/perf_engine.h"
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

static void test_vring_submit_wrap(void)
{
      struct {
            struct vring_avail hdr;
            uint16_t ring[16];
      } avail_buf;
      struct vring vr = {
            .avail = &avail_buf.hdr,
            .size = 16,
      };

      memset(&avail_buf, 0, sizeof(avail_buf));
      avail_buf.hdr.idx = UINT16_MAX;
      vring_submit(&vr, 7);
      CHECK(avail_buf.ring[15] == 7,
              "submission before wrap uses the final ring slot");
      CHECK(avail_buf.hdr.idx == 0, "available index wraps to zero");
}

static void test_vring_submit_batch(void)
{
      struct {
            struct vring_avail hdr;
            uint16_t ring[16];
      } avail_buf;
      struct vring vr = {
            .avail = &avail_buf.hdr,
            .size = 16,
      };
      const uint16_t heads[] = {3, 7, 11};

      memset(&avail_buf, 0, sizeof(avail_buf));
      avail_buf.hdr.idx = 15;
      vring_submit_batch(&vr, heads, 3);
      CHECK(avail_buf.ring[15] == 3 && avail_buf.ring[0] == 7 &&
              avail_buf.ring[1] == 11,
              "batch submission wraps across ring cells");
      CHECK(avail_buf.hdr.idx == 18,
              "batch publishes one final available index");
}

static void test_perf_slot_lifecycle(void)
{
    uint8_t memory;
    struct perf_request_slot slot;

    perf_slot_init(&slot, 3, 3, &memory);
    CHECK(slot.state == PERF_SLOT_FREE, "slot starts free");
    CHECK(slot.head == 3 && slot.id == 3, "slot identifiers are retained");
    CHECK(slot.workload_memory == &memory, "slot retains workload memory");
    CHECK(perf_slot_submit(&slot) < 0, "free slot submission is rejected");
    CHECK(perf_slot_prepare(&slot) == 0, "free slot can be prepared");
    CHECK(perf_slot_prepare(&slot) < 0, "prepared slot reuse is rejected");
    CHECK(perf_slot_submit(&slot) == 0, "prepared slot can be submitted");
    CHECK(perf_slot_complete(&slot, 4) < 0,
          "completion with another identifier is rejected");
    CHECK(perf_slot_complete(&slot, 3) == 0,
          "matching completion is accepted");
    CHECK(perf_slot_prepare(&slot) == 0, "completed slot can be prepared");
}

static void test_perf_slots_complete_by_identifier(void)
{
    struct perf_request_slot slots[2];

    perf_slot_init(&slots[0], 0, 0, NULL);
    perf_slot_init(&slots[1], 1, 1, NULL);
    CHECK(perf_slot_prepare(&slots[0]) == 0 &&
          perf_slot_submit(&slots[0]) == 0,
          "first slot is submitted");
    CHECK(perf_slot_prepare(&slots[1]) == 0 &&
          perf_slot_submit(&slots[1]) == 0,
          "second slot is submitted");
      CHECK(perf_slots_complete(slots, 2, 1) == 1,
          "second slot can complete first");
    CHECK(slots[0].state == PERF_SLOT_SUBMITTED &&
          slots[1].state == PERF_SLOT_COMPLETED,
          "completion changes only the matching slot");
    CHECK(perf_slot_prepare(&slots[0]) < 0,
          "outstanding slot cannot be reused after partial completion");
    CHECK(perf_slots_complete(slots, 2, 0) == 0,
          "first slot completes by identifier");
    CHECK(perf_slots_complete(slots, 2, 2) < 0,
          "unknown completion identifier is rejected");
}

static void test_perf_batch_accounting(void)
{
      const unsigned batch_sizes[] = {1, 4, 8, 16};

      for (unsigned test = 0; test < sizeof(batch_sizes) / sizeof(batch_sizes[0]);
             test++) {
            struct perf_run_stats stats;
            unsigned batch_size = batch_sizes[test];

            perf_stats_init(&stats);
            for (unsigned first = 0; first < 18; first += batch_size) {
                  unsigned count = batch_size;

                  if (count > 18 - first)
                        count = 18 - first;
                  perf_stats_submit(&stats, count);
                  for (unsigned completion = 0; completion < count; completion++)
                        perf_stats_complete(&stats);
            }
            CHECK(stats.submissions == 18 && stats.completions == 18,
                    "batch size %u counts 18 requests", batch_size);
            CHECK(stats.notifications == (18 + batch_size - 1) / batch_size,
                    "batch size %u counts partial batch notification", batch_size);
      }
}

static void test_perf_multi_queue_accounting(void)
{
      struct perf_run_stats stats;

      perf_stats_init(&stats);
      perf_stats_submit(&stats, 16);
      perf_stats_submit(&stats, 16);
      for (unsigned completion = 0; completion < 32; completion++)
            perf_stats_complete(&stats);
      perf_stats_submit(&stats, 16);
      for (unsigned completion = 0; completion < 16; completion++)
            perf_stats_complete(&stats);
      CHECK(stats.submissions == 48 && stats.completions == 48,
              "two queues and cleanup count every request");
      CHECK(stats.notifications == 3,
              "two queues and cleanup count each notification");
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
      test_vring_submit_wrap();
      test_vring_submit_batch();
            test_perf_slot_lifecycle();
            test_perf_slots_complete_by_identifier();
            test_perf_batch_accounting();
            test_perf_multi_queue_accounting();
    test_registry();
    int ok = (tests_passed == tests_run);
    const char *tag = ok ? c_pass : c_fail;
    printf("\n%sselftest/lib: %d/%d passed%s\n",
           tag, tests_passed, tests_run, c_reset);
    return ok ? 0 : 1;
}
