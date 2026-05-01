/* SPDX-License-Identifier: Apache-2.0 */
#ifndef VV_TEST_H
#define VV_TEST_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "../lib/virtio_pci.h"
#include "../lib/virtio_mmio.h"
#include "../lib/vring.h"
#include "../lib/vring_packed.h"

/*
 * Diagnostic helpers. TFAIL/TWEDGED/TREJECT emit a single line with the
 * source location and a printf-style reason, then yield the result.
 * Use in place of bare `return TEST_FAIL;` so console logs identify
 * which assertion fired.
 *
 * The "vv-*" prefixes deliberately differ from the "[FAIL]"/"[WEDGED]"/
 * "[REJECT]" verdict markers emitted by bin/init.c (which the host
 * runner scrapes to decide the test outcome). If we reused those
 * prefixes the runner would treat the diagnostic as the verdict and
 * kill the VMM mid-line.
 */
#define TFAIL(fmt, ...) do { \
    printf("vv-fail %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
    fflush(stdout); \
    return TEST_FAIL; \
} while (0)

#define TWEDGED(fmt, ...) do { \
    printf("vv-wedged %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
    fflush(stdout); \
    return TEST_WEDGED; \
} while (0)

#define TREJECT(fmt, ...) do { \
    printf("vv-reject %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
    fflush(stdout); \
    return TEST_REJECT; \
} while (0)

typedef enum {
    TEST_PASS,
    TEST_FAIL,
    TEST_SKIP,
    TEST_WEDGED,
    TEST_REJECT,
    /*
     * Synthetic verdicts emitted by init when a test was registered
     * with TEST_FLAG_XFAIL. Tests never return these directly; init
     * remaps the test's PASS/FAIL/REJECT/WEDGED outcome.
     */
    TEST_XFAIL,
    TEST_XPASS
} test_result_t;

/* Default timeout for vv_kick_and_wait (ms). */
#define VV_TIMEOUT_MS 2000

typedef test_result_t (*test_fn)(struct virtio_dev *dev, struct vring *vr);
typedef test_result_t (*test_packed_fn)(struct virtio_dev *dev,
                                       struct vring_packed *vr);
typedef test_result_t (*test_mmio_fn)(struct virtio_mmio_dev *dev);

/* Test flags */
#define TEST_FLAG_PACKED  1
#define TEST_FLAG_MMIO    2
/*
 * Expected-fail marker. A test with this flag inverts its verdict:
 * a non-PASS outcome (FAIL/REJECT/WEDGED) becomes XFAIL (counted as
 * success), and PASS becomes XPASS (counted as a failure, signaling
 * that the underlying bug is fixed and the marker should be removed).
 */
#define TEST_FLAG_XFAIL   4

#define VIRTIO_SPEC_VERSION(major, minor) (((major) << 8) | (minor))
#define VIRTIO_SPEC_V1_2  VIRTIO_SPEC_VERSION(1, 2)
#define VIRTIO_SPEC_V1_3  VIRTIO_SPEC_VERSION(1, 3)
#define VIRTIO_SPEC_V1_4  VIRTIO_SPEC_VERSION(1, 4)

struct test_entry {
    const char   *name;
    const char   *desc;
    uint16_t      spec_version;
    const char   *spec_section;
    uint16_t      device_id;
    void         *fn;
    uint8_t       flags;
    uint8_t       queue_idx;   /* 0 = default, 1+ = explicit queue (value-1), 0xFF = last */
    char          _pad[14];
};

/* Use with REGISTER_TEST_Q to select the last queue (e.g. net controlq). */
#define VV_QUEUE_LAST 0xFE

/*
 * Register a test case. The linker collects all entries into a
 * contiguous array via the "test_registry" section.
 * aligned(64) ensures consistent stride across translation units.
 */
#define REGISTER_TEST(tname, dev_id, func, description, specver, sect) \
    __attribute__((section("test_registry"), used, aligned(64))) \
    static struct test_entry _test_##tname = { \
        #tname, description, specver, sect, dev_id, (void *)(func), 0, 0, {0} \
    }

#define REGISTER_TEST_Q(tname, dev_id, func, description, specver, sect, qidx) \
    __attribute__((section("test_registry"), used, aligned(64))) \
    static struct test_entry _test_##tname = { \
        #tname, description, specver, sect, dev_id, (void *)(func), 0, \
        (qidx) + 1, {0} \
    }

#define REGISTER_TEST_PACKED(tname, dev_id, func, description, specver, sect) \
    __attribute__((section("test_registry"), used, aligned(64))) \
    static struct test_entry _test_##tname = { \
        #tname, description, specver, sect, dev_id, (void *)(func), \
        TEST_FLAG_PACKED, 0, {0} \
    }

#define REGISTER_TEST_MMIO(tname, func, description, specver, sect) \
    __attribute__((section("test_registry"), used, aligned(64))) \
    static struct test_entry _test_##tname = { \
        #tname, description, specver, sect, 0, \
        (void *)(func), TEST_FLAG_MMIO, 0, {0} \
    }

/*
 * Same as REGISTER_TEST but marks the test as expected to fail. Use
 * for tests that document a known VMM bug; remove the _XFAIL suffix
 * once the bug is fixed (an XPASS will then flag the registration as
 * stale).
 */
#define REGISTER_TEST_XFAIL(tname, dev_id, func, description, specver, sect) \
    __attribute__((section("test_registry"), used, aligned(64))) \
    static struct test_entry _test_##tname = { \
        #tname, description, specver, sect, dev_id, (void *)(func), \
        TEST_FLAG_XFAIL, 0, {0} \
    }

/*
 * Kick a queue and wait for the device to mark n descriptors used.
 * Returns TEST_PASS once vr->used->idx advances by at least n,
 * TEST_REJECT if the device stayed silent but is still alive,
 * TEST_WEDGED if the device is no longer healthy (needs reset).
 *
 * Use this for batched submissions where the caller inspects the
 * tail of every request after the wait. Plain vv_kick_and_wait
 * returns on the first completion and is fine for single chain
 * submissions.
 */
static inline test_result_t vv_kick_and_wait_n(struct virtio_dev *dev,
                                               struct vring *vr,
                                               uint16_t queue,
                                               uint16_t n,
                                               int timeout_ms)
{
    uint16_t before = vr->used->idx;
    __sync_synchronize();
    virtio_pci_kick(dev, vr->queue);

    (void)queue;

    int elapsed = 0;
    int step = 10000; /* 10ms */
    while (elapsed < timeout_ms * 1000) {
        usleep(step);
        __sync_synchronize();
        if ((uint16_t)(vr->used->idx - before) >= n)
            return TEST_PASS;
        elapsed += step;
    }

    /* Timeout - check if device is still alive */
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    __sync_synchronize();
    uint8_t status = cfg->device_status;
    if (status == 0)
        return TEST_WEDGED;
    return TEST_REJECT;
}

/*
 * Kick a queue and wait for the device to consume the request.
 * Returns TEST_PASS if the used ring advanced (device processed it),
 * TEST_REJECT if the device stayed silent but is still alive,
 * TEST_WEDGED if the device is no longer healthy (needs reset).
 */
static inline test_result_t vv_kick_and_wait(struct virtio_dev *dev,
                                             struct vring *vr,
                                             uint16_t queue, int timeout_ms)
{
    return vv_kick_and_wait_n(dev, vr, queue, 1, timeout_ms);
}

/*
 * Kick and expect no response (device should reject the request).
 * Returns TEST_REJECT if the device stays silent but remains alive,
 * TEST_PASS if the device unexpectedly responds (processed it anyway),
 * TEST_FAIL if the device becomes unresponsive on a follow-up probe.
 */
static inline test_result_t vv_kick_expect_reject(struct virtio_dev *dev,
                                                  struct vring *vr,
                                                  int timeout_ms)
{
    uint16_t before = vr->used->idx;
    __sync_synchronize();
    virtio_pci_kick(dev, vr->queue);

    int elapsed = 0;
    int step = 10000;
    while (elapsed < timeout_ms * 1000) {
        usleep(step);
        __sync_synchronize();
        if (vr->used->idx != before)
            return TEST_PASS; /* device processed it (unexpected) */
        elapsed += step;
    }

    /* Device didn't respond - verify it's still alive by reading status */
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    __sync_synchronize();
    uint8_t status = cfg->device_status;
    if (status == 0)
        return TEST_WEDGED;

    return TEST_REJECT;
}

/*
 * Kick a packed queue and wait for the device to mark a descriptor used.
 * Returns TEST_PASS if the device used the descriptor at check_idx,
 * TEST_REJECT if the device stayed silent but is still alive,
 * TEST_WEDGED if the device is no longer healthy (needs reset).
 */
static inline test_result_t vv_kick_and_wait_packed(struct virtio_dev *dev,
                                                    struct vring_packed *vr,
                                                    uint16_t queue,
                                                    uint16_t check_idx,
                                                    uint8_t check_wrap,
                                                    int timeout_ms)
{
    (void)queue;
    __sync_synchronize();
    virtio_pci_kick(dev, vr->queue);

    int elapsed = 0;
    int step = 10000; /* 10ms */
    while (elapsed < timeout_ms * 1000) {
        usleep(step);
        if (vring_packed_desc_is_used(vr, check_idx, check_wrap))
            return TEST_PASS;
        elapsed += step;
    }

    /* Timeout - check if device is still alive */
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    __sync_synchronize();
    uint8_t status = cfg->device_status;
    if (status == 0)
        return TEST_WEDGED;
    return TEST_REJECT;
}

/* Linker-defined boundaries for the test registry section. */
extern struct test_entry __start_test_registry;
extern struct test_entry __stop_test_registry;

static inline int test_count(void)
{
    return &__stop_test_registry - &__start_test_registry;
}

static inline struct test_entry *test_get(int i)
{
    return &__start_test_registry + i;
}

static inline struct test_entry *test_find(const char *name)
{
    int n = test_count();
    for (int i = 0; i < n; i++) {
        struct test_entry *t = test_get(i);
        if (strcmp(t->name, name) == 0)
            return t;
    }
    return NULL;
}

#endif /* VV_TEST_H */
