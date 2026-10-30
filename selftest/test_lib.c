/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Selftests for the vring library.
 * Runs on the host - no VM needed. Tests MMIO accessors, ring
 * manipulation, registry lookup, and performance accounting.
 */
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../lib/perf_engine.h"
#include "../lib/vring.h"
#include "../lib/vring_packed.h"
#include "../tests/test.h"

/*
 * The ring tests back ring headers that end in a flexible array with a
 * fixed array placed right after them. GNU C allows this, but Clang
 * warns about it by default.
 */
#ifdef __clang__
#pragma clang diagnostic ignored "-Wgnu-variable-sized-type-not-at-end"
#endif

static int tests_run;
static int tests_passed;
static const char *c_pass = "";
static const char *c_fail = "";
static const char *c_reset = "";

#define CHECK(cond, fmt, ...) do { \
    tests_run++; \
    if (!(cond)) { \
        printf("  %s[FAIL]%s " fmt "\n", \
               c_fail, c_reset __VA_OPT__(,) __VA_ARGS__); \
    } else { \
        printf("  %s[PASS]%s " fmt "\n", \
               c_pass, c_reset __VA_OPT__(,) __VA_ARGS__); \
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

/* --- MMIO accessors --- */

static void test_mmio_accesses(void)
{
    volatile uint8_t bytes[] = { 0x5a, 0, 0xa5 };
    int matches = 1;
    for (unsigned i = 0; i < 256; i++) {
        bytes[1] = (uint8_t)i;
        if (vv_mmio_read8(bytes + 1) != i)
            matches = 0;
        vv_mmio_write8(bytes + 1, (uint8_t)(255 - i));
        if (bytes[1] != 255 - i)
            matches = 0;
    }
    CHECK(matches, "MMIO byte reads and writes preserve all 256 values");
    CHECK(bytes[0] == 0x5a && bytes[2] == 0xa5,
          "MMIO byte writes preserve adjacent bytes");

    volatile uint16_t halves[] = { 0x5aa5, 0, 0xa55a };
    static const uint16_t values16[] = { 0, 1, 0x1234, 0x8000, 0xffff };
    matches = 1;
    for (unsigned i = 0; i < sizeof(values16) / sizeof(values16[0]); i++) {
        halves[1] = values16[i];
        if (vv_mmio_read16(halves + 1) != values16[i])
            matches = 0;
        vv_mmio_write16(halves + 1, values16[i] ^ 0xffff);
        if (halves[1] != (uint16_t)(values16[i] ^ 0xffff))
            matches = 0;
    }
    CHECK(matches, "MMIO 16 bit reads and writes preserve values");
    CHECK(halves[0] == 0x5aa5 && halves[2] == 0xa55a,
          "MMIO 16 bit writes preserve adjacent fields");

    volatile uint32_t words[] = { 0x5aa55aa5, 0, 0xa55aa55a };
    static const uint32_t values32[] = {
        0, 1, 0x12345678, 0x80000000, 0xffffffff,
    };
    matches = 1;
    for (unsigned i = 0; i < sizeof(values32) / sizeof(values32[0]); i++) {
        words[1] = values32[i];
        if (vv_mmio_read32(words + 1) != values32[i])
            matches = 0;
        vv_mmio_write32(words + 1, ~values32[i]);
        if (words[1] != ~values32[i])
            matches = 0;
    }
    CHECK(matches, "MMIO 32 bit reads and writes preserve values");
    CHECK(words[0] == 0x5aa55aa5 && words[2] == 0xa55aa55a,
          "MMIO 32 bit writes preserve adjacent fields");
}

static void test_mmio_read_bytes(void)
{
    volatile uint8_t source[256];
    uint8_t copy[258];

    for (unsigned i = 0; i < sizeof(source); i++)
        source[i] = (uint8_t)i;
    memset(copy, 0xa5, sizeof(copy));
    vv_mmio_read_bytes(copy + 1, source, sizeof(source));

    int matches = 1;
    for (unsigned i = 0; i < sizeof(source); i++) {
        if (copy[i + 1] != (uint8_t)i)
            matches = 0;
    }
    CHECK(matches, "MMIO byte copy preserves all 256 byte values");
    CHECK(copy[0] == 0xa5 && copy[257] == 0xa5,
          "MMIO byte copy preserves destination boundaries");

    source[0] = 0x5a;
    vv_mmio_read_bytes(copy + 1, source, 1);
    CHECK(copy[1] == 0x5a && copy[2] == 1,
          "MMIO byte copy observes an updated value and exact length");
    vv_mmio_read_bytes(copy + 1, source, 0);
    CHECK(copy[1] == 0x5a, "zero length MMIO byte copy leaves output unchanged");
}

static void test_mmio_wrappers(void)
{
    volatile uint8_t bytes[] = { 0x5a, 0x81, 0xa5 };
    volatile uint16_t halves[] = { 0x5aa5, 0x8123, 0xa55a };
    volatile uint32_t words[] = { 0x5aa55aa5, 0x81234567, 0xa55aa55a };
    struct virtio_mmio_dev dev = { .base = bytes };
    CHECK(mmio_read8(&dev, 1) == 0x81,
          "MMIO byte register read uses the requested offset");
    mmio_write8(&dev, 1, 0xff);
    CHECK(bytes[0] == 0x5a && bytes[1] == 0xff && bytes[2] == 0xa5,
          "MMIO byte register write preserves width and offset");
    dev.base = halves;
    CHECK(mmio_read16(&dev, 2) == 0x8123,
          "MMIO 16 bit register read uses the requested offset");
    mmio_write16(&dev, 2, 0xffff);
    CHECK(halves[0] == 0x5aa5 && halves[1] == 0xffff && halves[2] == 0xa55a,
          "MMIO 16 bit register write preserves width and offset");
    dev.base = words;
    CHECK(mmio_read32(&dev, 4) == 0x81234567,
          "MMIO 32 bit register read uses the requested offset");
    mmio_write32(&dev, 4, 0xffffffff);
    CHECK(words[0] == 0x5aa55aa5 && words[1] == 0xffffffff &&
          words[2] == 0xa55aa55a,
          "MMIO 32 bit register write preserves width and offset");

    volatile uint32_t split[] = { 0x5aa55aa5, 0, 0, 0xa55aa55a };
    virtio_store64(split + 1, UINT64_C(0xfedcba9876543210));
    CHECK(split[1] == 0x76543210 && split[2] == 0xfedcba98 &&
          split[0] == 0x5aa55aa5 && split[3] == 0xa55aa55a,
          "PCI 64 bit store preserves low then high word layout");
    split[1] = 0x89abcdef;
    split[2] = 0x01234567;
    CHECK(virtio_load64(split + 1) == UINT64_C(0x0123456789abcdef),
          "PCI 64 bit load combines low and high words");

    struct virtio_pci_common_cfg cfg = { .device_feature = 0x80000001 };
    struct virtio_dev pci = { .common = &cfg };
    CHECK(virtio_pci_feature_offered(&pci, 63) &&
          cfg.device_feature_select == 1,
          "PCI feature accessor selects the high feature word");
    CHECK(!virtio_pci_feature_offered(&pci, 1) &&
          cfg.device_feature_select == 0,
          "PCI feature accessor selects the low word and masks the bit");
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

      static void test_vring_event_idx(void)
      {
            struct {
                struct vring_used hdr;
                struct vring_used_elem ring[16];
                uint16_t avail_event;
            } used_buf;
            struct vring vr = {
                .used = &used_buf.hdr,
                .size = 16,
            };

            memset(&used_buf, 0, sizeof(used_buf));
            used_buf.avail_event = 7;
            CHECK(vring_avail_event(&vr) == 7,
                  "available event follows used ring entries");
            CHECK(vring_need_event(7, 8, 4),
                  "event index requests notification at the threshold");
            CHECK(!vring_need_event(8, 8, 4),
                  "event index suppresses notification below the threshold");
            CHECK(vring_need_event(UINT16_MAX, 0, UINT16_MAX),
                  "event index notification handles index wrap");
            CHECK(!vring_need_event(0, 0, UINT16_MAX),
                  "event index suppression handles index wrap");
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

static void test_vring_packed_submit_chain(void)
{
      struct vring_packed_desc ring[4] = {0};
      struct vring_packed vr = {
          .desc = ring,
          .size = 4,
          .next_avail = 3,
          .wrap_counter = 1,
      };
      const struct vring_packed_desc chain[3] = {
          {.addr = 1, .len = 10, .flags = VRING_PACKED_DESC_F_NEXT},
          {.addr = 2, .len = 20, .flags = VRING_PACKED_DESC_F_NEXT |
                                     VRING_PACKED_DESC_F_WRITE},
          {.addr = 3, .len = 30, .flags = VRING_PACKED_DESC_F_WRITE},
      };
      uint16_t head;
      uint8_t wrap;

      CHECK(vring_packed_submit_chain(&vr, chain, 3, 7, &head, &wrap) == 0,
            "packed chain submission succeeds");
      CHECK(head == 3 && wrap == 1,
            "packed chain retains its head phase");
      CHECK(vr.next_avail == 2 && vr.wrap_counter == 0,
            "packed chain advances across ring wrap");
      CHECK(ring[3].id == 7 && ring[0].id == 7 && ring[1].id == 7,
            "packed chain uses one request identifier");
      CHECK((ring[3].flags & VRING_PACKED_DESC_F_AVAIL) != 0 &&
            (ring[3].flags & VRING_PACKED_DESC_F_USED) == 0,
            "packed head is available in its initial phase");
      CHECK((ring[0].flags & VRING_PACKED_DESC_F_AVAIL) == 0 &&
            (ring[0].flags & VRING_PACKED_DESC_F_USED) != 0,
            "packed tail uses the wrapped phase");
      CHECK(vring_packed_submit_chain(&vr, chain, 0, 7, &head, &wrap) < 0,
            "empty packed chain is rejected");
            vr.next_used = 3;
            vr.used_wrap_counter = 1;
            ring[3].flags = VRING_PACKED_DESC_F_AVAIL |
                        VRING_PACKED_DESC_F_USED;
            ring[3].id = 9;
            ring[3].len = 99;
            uint16_t id;
            uint32_t len;
            CHECK(vring_packed_next_used(&vr, &id, &len) == 0 &&
                  id == 9 && len == 99,
                  "packed used cursor reads completion identity");
            vring_packed_advance_used(&vr, 3);
            CHECK(vr.next_used == 2 && vr.used_wrap_counter == 0,
                  "packed used cursor advances across ring wrap");

                  memset(ring, 0, sizeof(ring));
                  vr.next_avail = 0;
                  vr.wrap_counter = 1;
                  for (unsigned request = 0; request < 12; request++)
                      CHECK(vring_packed_submit_chain(&vr, chain, 1, request,
                                               &head, &wrap) == 0,
                            "packed submission %u completes", request + 1);
                  CHECK(vr.next_avail == 0 && vr.wrap_counter == 0,
                        "packed submission survives three complete ring rotations");
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
                  perf_stats_submit(&stats, count, true);
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
      perf_stats_submit(&stats, 16, true);
      perf_stats_submit(&stats, 16, false);
      for (unsigned completion = 0; completion < 32; completion++)
            perf_stats_complete(&stats);
      perf_stats_submit(&stats, 16, true);
      for (unsigned completion = 0; completion < 16; completion++)
            perf_stats_complete(&stats);
      CHECK(stats.submissions == 48 && stats.completions == 48,
              "two queues and cleanup count every request");
            CHECK(stats.notifications == 2,
                  "two queues and cleanup count actual notifications");
}

static void test_vring_prepare(void)
{
    struct virtio_pci_common_cfg cfg = {0};
    struct virtio_dev dev = { .common = &cfg };
    struct vring_avail avail = {0};
    struct vring vr = {
        .avail = &avail,
        .desc_phys = 0x1234567800001000ULL,
        .avail_phys = 0x2345678900002000ULL,
        .used_phys = 0x3456789a00003000ULL,
        .size = 8,
    };

    vring_prepare(&dev, &vr, 3);
    CHECK(vr.queue == 3 && cfg.queue_select == 3,
          "vring_prepare selects the requested queue");
    CHECK(cfg.queue_size == 8, "vring_prepare sets queue size");
    CHECK(virtio_load64(&cfg.queue_desc) == vr.desc_phys,
          "vring_prepare sets descriptor address");
    CHECK(virtio_load64(&cfg.queue_avail) == vr.avail_phys,
          "vring_prepare sets available ring address");
    CHECK(virtio_load64(&cfg.queue_used) == vr.used_phys,
          "vring_prepare sets used ring address");
    CHECK(cfg.queue_msix_vector == 0xffff,
          "vring_prepare defaults to no MSI-X vector");
    CHECK(avail.flags == VRING_AVAIL_F_NO_INTERRUPT,
          "vring_prepare suppresses interrupts by default");
    CHECK(cfg.queue_enable == 0, "vring_prepare leaves the queue disabled");

    vring_attach(&dev, &vr, 2);
    CHECK(vr.queue == 2 && cfg.queue_select == 2,
          "vring_attach still selects the requested queue");
    CHECK(cfg.queue_enable == 1, "vring_attach still enables the queue");
    CHECK(cfg.queue_msix_vector == 0xffff &&
          avail.flags == VRING_AVAIL_F_NO_INTERRUPT,
          "vring_attach still suppresses interrupts");
}

static void test_base_features(void)
{
    struct virtio_pci_common_cfg cfg = {0};
    struct virtio_dev dev = { .common = &cfg };
    const uint32_t version = 1u << (VIRTIO_F_VERSION_1 - 32);
    const uint32_t access = 1u << (VIRTIO_F_ACCESS_PLATFORM - 32);

    CHECK(virtio_pci_base_features_hi(&dev) == 0,
          "base features do not add unoffered bits");
    CHECK(cfg.device_feature_select == 1,
          "base features read feature word one");
    cfg.device_feature = version;
    CHECK(virtio_pci_base_features_hi(&dev) == version,
          "base features accept VERSION_1 without ACCESS_PLATFORM");
    cfg.device_feature = version | access;
    CHECK(virtio_pci_base_features_hi(&dev) == (version | access),
          "base features accept offered ACCESS_PLATFORM");
    cfg.device_feature = access;
    CHECK(virtio_pci_base_features_hi(&dev) == access,
          "base features do not invent VERSION_1");
    cfg.device_feature = UINT32_MAX;
    CHECK(virtio_pci_base_features_hi(&dev) == (version | access),
          "base features exclude packed rings and other optional features");
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
    test_mmio_accesses();
    test_mmio_read_bytes();
    test_mmio_wrappers();
    test_raw_set_desc();
    test_raw_set_avail();
    test_vring_prepare();
      test_vring_submit_wrap();
      test_vring_submit_batch();
      test_vring_event_idx();
      test_vring_packed_submit_chain();
            test_perf_slot_lifecycle();
            test_perf_slots_complete_by_identifier();
            test_perf_batch_accounting();
            test_perf_multi_queue_accounting();
    test_registry();
    test_base_features();
    int ok = (tests_passed == tests_run);
    const char *tag = ok ? c_pass : c_fail;
    printf("\n%sselftest/lib: %d/%d passed%s\n",
           tag, tests_passed, tests_run, c_reset);
    return ok ? 0 : 1;
}
