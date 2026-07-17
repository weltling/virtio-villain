/* SPDX-License-Identifier: Apache-2.0 */
/*
 * L0042: stats queue accepts multiple tags in arbitrary order.
 *
 * Spec 5.5.6.2 (device-normative): within an output buffer submitted
 * to the statsq, the device MUST accept struct virtio_balloon_stat
 * entries in any order without regard to tag values. Submit one
 * buffer holding several well-known stat tags in a deliberately
 * unsorted order and verify the device consumes it.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_balloon_stats_any_order(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_BALLOON_F_STATS_VQ)))
        return TEST_SKIP;

    cfg->queue_select = 1;
    __sync_synchronize();
    if (cfg->queue_size == 0)
        return TEST_SKIP;

    struct vring sq;
    vring_alloc(&sq, 16);
    vring_attach(dev, &sq, 1);

    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    /* Five tags in an order that is not sorted by tag value. */
    static const uint16_t tags[] = {
        VIRTIO_BALLOON_S_MEMTOT,   /* 5 */
        VIRTIO_BALLOON_S_SWAP_IN,  /* 0 */
        VIRTIO_BALLOON_S_AVAIL,    /* 6 */
        VIRTIO_BALLOON_S_MEMFREE,  /* 4 */
        VIRTIO_BALLOON_S_CACHES,   /* 7 */
    };
    const unsigned n = sizeof(tags) / sizeof(tags[0]);

    struct virtio_balloon_stat *entries = vv_alloc_pages(1);
    for (unsigned i = 0; i < n; i++) {
        entries[i].tag = tags[i];
        entries[i].val = (uint64_t)(i + 1) << 20;
    }

    vring_raw_set_desc(&sq, 0, vv_virt_to_phys(entries),
                       n * sizeof(struct virtio_balloon_stat), 0, 0);
    vring_raw_set_avail(&sq, 0, 0);
    vring_raw_set_avail_idx(&sq, 1);

    return vv_kick_and_wait(dev, &sq, 1, VV_TIMEOUT_MS);
}

REGISTER_TEST_REQUIRES(L0042, VIRTIO_PCI_DEVICE_BALLOON,
              test_balloon_stats_any_order,
              "Stats queue accepts multiple tags in arbitrary order",
              VIRTIO_SPEC_V1_2, "5.5.6.2",
              (1ULL << VIRTIO_BALLOON_F_STATS_VQ), 0);
