/* SPDX-License-Identifier: Apache-2.0 */
#ifndef VV_TEST_H
#define VV_TEST_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "../lib/util.h"
#include "../lib/virtio_pci.h"
#include "../lib/virtio_mmio.h"
#include "../lib/vring.h"
#include "../lib/vring_packed.h"

#include "../lib/test.h"

/* Return the result of expr from the calling test unless it is TEST_PASS. */
#define VV_TRY(expr) do { \
    test_result_t vv_try_r_ = (expr); \
    if (vv_try_r_ != TEST_PASS) \
        return vv_try_r_; \
} while (0)

/* Default timeout for vv_kick_and_wait (ms). */
#define VV_TIMEOUT_MS 2000

/*
 * Multiplier applied to the positive wait timeout. The runner sets it
 * from vv.wait_scale on the kernel command line to the host
 * oversubscription factor, so a device response that arrives late
 * because the guest vCPUs are starved is still caught instead of being
 * misread as silence. It defaults to 1, so an unloaded run is
 * unchanged. Only the positive wait helpers apply it; the expect reject
 * path keeps the short silence window.
 */
extern int vv_wait_scale;

typedef test_result_t (*test_fn)(struct virtio_dev *dev, struct vring *vr);
typedef test_result_t (*test_dual_q_fn)(struct virtio_dev *dev,
                                       struct vring *vr,
                                       struct vring *ctrl_vr);
typedef test_result_t (*test_packed_fn)(struct virtio_dev *dev,
                                       struct vring_packed *vr);
typedef test_result_t (*test_mmio_fn)(struct virtio_mmio_dev *dev);
typedef test_result_t (*test_dev_fn)(struct virtio_dev *dev);

struct mmio_test_variant {
    const char *name;
    test_mmio_fn fn;
    uint32_t device_id;
};

#define REGISTER_MMIO_VARIANT(tname, dev_id, func) \
    __attribute__((section("mmio_variants"), used, aligned(8))) \
    static const struct mmio_test_variant _mmio_##tname = { #tname, func, dev_id }

/* Test flags */
#define TEST_FLAG_PACKED    1
#define TEST_FLAG_MMIO      2
#define TEST_FLAG_XFAIL     4
#define TEST_FLAG_NEEDS_ISR 8
#define TEST_FLAG_NEEDS_CFG 16
#define TEST_FLAG_DUAL_Q    32
#define TEST_FLAG_CONTROL_Q 64
#define TEST_FLAG_DEV_ONLY  128
/*
 * Expected-fail marker. A test with this flag inverts its verdict:
 * a non-PASS outcome (FAIL/REJECT/WEDGED) becomes XFAIL (counted as
 * success), and PASS becomes XPASS (counted as a failure, signaling
 * that the underlying bug is fixed and the marker should be removed).
 */

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
    uint16_t      min_queues;  /* 0 = no requirement */
    unsigned __int128 required_features; /* feature bits that must be offered */
    char          _pad[40];
};

/* Use with REGISTER_TEST_Q to select the last queue (e.g. net controlq). */
#define VV_QUEUE_LAST 0xFE

/*
 * Feature mask helper for a single bit. Use for required-feature masks
 * that reference a bit at 64 or above (e.g. VIRTIO_NET_F_RSS_CONTEXT),
 * where a plain 1ULL << bit would overflow. Bits below 64 may keep the
 * 1ULL << bit form.
 */
#define VV_FEATURE_BIT(bit) ((unsigned __int128)1u << (bit))

/*
 * Register a test case. The linker collects all entries into a
 * contiguous array via the "test_registry" section.
 * aligned(128) ensures consistent stride across translation units.
 */
#define REGISTER_TEST(tname, dev_id, func, description, specver, sect) \
    __attribute__((section("test_registry"), used, aligned(128))) \
    static struct test_entry _test_##tname = { \
        #tname, description, specver, sect, dev_id, (void *)(func), 0, 0, 0, 0, {0} \
    }

#define REGISTER_TEST_Q(tname, dev_id, func, description, specver, sect, qidx) \
    __attribute__((section("test_registry"), used, aligned(128))) \
    static struct test_entry _test_##tname = { \
        #tname, description, specver, sect, dev_id, (void *)(func), 0, \
        (qidx) + 1, 0, 0, {0} \
    }

#define REGISTER_TEST_DUAL_Q(tname, dev_id, func, description, specver, sect) \
    __attribute__((section("test_registry"), used, aligned(128))) \
    static struct test_entry _test_##tname = { \
        #tname, description, specver, sect, dev_id, (void *)(func), \
        TEST_FLAG_DUAL_Q, 0, 2, 0, {0} \
    }

#define REGISTER_TEST_CONTROL_Q(tname, dev_id, func, description, specver, \
                                sect, qidx) \
    __attribute__((section("test_registry"), used, aligned(128))) \
    static struct test_entry _test_##tname = { \
        #tname, description, specver, sect, dev_id, (void *)(func), \
        TEST_FLAG_CONTROL_Q, (qidx) + 1, (qidx) + 1, 0, {0} \
    }

#define REGISTER_TEST_PACKED(tname, dev_id, func, description, specver, sect) \
    __attribute__((section("test_registry"), used, aligned(128))) \
    static struct test_entry _test_##tname = { \
        #tname, description, specver, sect, dev_id, (void *)(func), \
        TEST_FLAG_PACKED, 0, 0, (1ULL << VIRTIO_F_RING_PACKED), {0} \
    }

#define REGISTER_TEST_MMIO(tname, func, description, specver, sect) \
    __attribute__((section("test_registry"), used, aligned(128))) \
    static struct test_entry _test_##tname = { \
        #tname, description, specver, sect, 0, \
        (void *)(func), TEST_FLAG_MMIO, 0, 0, 0, {0} \
    }

/*
 * Same as REGISTER_TEST but marks the test as expected to fail. Use
 * for tests that document a known VMM bug; remove the _XFAIL suffix
 * once the bug is fixed (an XPASS will then flag the registration as
 * stale).
 */
#define REGISTER_TEST_XFAIL(tname, dev_id, func, description, specver, sect) \
    __attribute__((section("test_registry"), used, aligned(128))) \
    static struct test_entry _test_##tname = { \
        #tname, description, specver, sect, dev_id, (void *)(func), \
        TEST_FLAG_XFAIL, 0, 0, 0, {0} \
    }

#define REGISTER_TEST_Q_XFAIL(tname, dev_id, func, description, specver, sect, qidx) \
    __attribute__((section("test_registry"), used, aligned(128))) \
    static struct test_entry _test_##tname = { \
        #tname, description, specver, sect, dev_id, (void *)(func), \
        TEST_FLAG_XFAIL, (qidx) + 1, 0, 0, {0} \
    }

/*
 * Register a test with required feature bits and minimum queue count.
 * The probe can pre-filter tests whose requirements are not met.
 */
#define REGISTER_TEST_REQUIRES(tname, dev_id, func, description, specver, \
                               sect, features, minq) \
    __attribute__((section("test_registry"), used, aligned(128))) \
    static struct test_entry _test_##tname = { \
        #tname, description, specver, sect, dev_id, (void *)(func), \
        0, 0, (minq), (features), {0} \
    }

/*
 * Same as REGISTER_TEST and REGISTER_TEST_REQUIRES for a test that
 * drives no queue. func is a test_dev_fn, which the conditional checks
 * at compile time. The runner still sets up the queues as usual.
 */
#define REGISTER_TEST_DEV(tname, dev_id, func, description, specver, sect) \
    REGISTER_TEST_DEV_REQUIRES(tname, dev_id, func, description, specver, \
                               sect, 0, 0)

#define REGISTER_TEST_DEV_REQUIRES(tname, dev_id, func, description, \
                                   specver, sect, features, minq) \
    __attribute__((section("test_registry"), used, aligned(128))) \
    static struct test_entry _test_##tname = { \
        #tname, description, specver, sect, dev_id, \
        (void *)(1 ? (func) : (test_dev_fn)0), TEST_FLAG_DEV_ONLY, 0, \
        (minq), (features), {0} \
    }

#define REGISTER_TEST_FLAGS(tname, dev_id, func, description, specver, \
                            sect, fl) \
    __attribute__((section("test_registry"), used, aligned(128))) \
    static struct test_entry _test_##tname = { \
        #tname, description, specver, sect, dev_id, (void *)(func), \
        (fl), 0, 0, 0, {0} \
    }

#define REGISTER_TEST_Q_REQUIRES(tname, dev_id, func, description, specver, \
                                 sect, qidx, features, minq) \
    __attribute__((section("test_registry"), used, aligned(128))) \
    static struct test_entry _test_##tname = { \
        #tname, description, specver, sect, dev_id, (void *)(func), \
        0, (qidx) + 1, (minq), (features), {0} \
    }

/* vring_raw_set_desc() for a descriptor that points at a guest buffer. */
static inline void vv_desc(struct vring *vr, uint16_t slot, void *buf,
                           uint32_t len, uint16_t flags, uint16_t next)
{
    vring_raw_set_desc(vr, slot, vv_virt_to_phys(buf), len, flags, next);
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

    int scale = vv_wait_scale > 0 ? vv_wait_scale : 1;
    int budget_us = timeout_ms * 1000 * scale;
    int elapsed = 0;
    int step = 10000; /* 10ms */
    while (elapsed < budget_us) {
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
 * Make head the next available entry, then kick and wait like
 * vv_kick_and_wait() with the default timeout. On a fresh ring this
 * is the same as setting avail slot 0 to head and avail idx to 1.
 */
static inline test_result_t vv_submit_and_wait(struct virtio_dev *dev,
                                               struct vring *vr,
                                               uint16_t head)
{
    vring_submit(vr, head);
    return vv_kick_and_wait(dev, vr, vr->queue, VV_TIMEOUT_MS);
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

    int scale = vv_wait_scale > 0 ? vv_wait_scale : 1;
    int budget_us = timeout_ms * 1000 * scale;
    int elapsed = 0;
    int step = 10000; /* 10ms */
    while (elapsed < budget_us) {
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

/*
 * Allocate a blk request header with the given type and sector. The
 * remaining fields are zero. Many non blk tests use blk requests as
 * their vehicle, so this lives here and not in a blk helper header.
 */
static inline struct virtio_blk_outhdr *blk_hdr(uint32_t type,
                                                uint64_t sector)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);

    hdr->type = type;
    hdr->sector = sector;
    return hdr;
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
