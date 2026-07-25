/* SPDX-License-Identifier: Apache-2.0 */
/*
 * L0043: stats queue accepts the remaining well-known tags.
 *
 * Spec 5.5.6.2: the driver reports memory statistics as
 * struct virtio_balloon_stat (tag, val) pairs on the statsq. l0041
 * submits MEMFREE and l0042 submits MEMTOT/SWAP_IN/AVAIL/MEMFREE/
 * CACHES. This submits the tags not exercised elsewhere, SWAP_OUT,
 * MAJFLT, MINFLT, HTLB_PGALLOC and HTLB_PGFAIL, so every defined tag
 * is covered, and verifies the device consumes the buffer.
 * Skips when the device does not offer the stats queue.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_balloon_stats_remaining_tags(struct virtio_dev *dev,
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

    static const uint16_t tags[] = {
        VIRTIO_BALLOON_S_SWAP_OUT,     /* 1 */
        VIRTIO_BALLOON_S_MAJFLT,       /* 2 */
        VIRTIO_BALLOON_S_MINFLT,       /* 3 */
        VIRTIO_BALLOON_S_HTLB_PGALLOC, /* 8 */
        VIRTIO_BALLOON_S_HTLB_PGFAIL,  /* 9 */
    };
    const unsigned n = sizeof(tags) / sizeof(tags[0]);

    struct virtio_balloon_stat *entries = vv_alloc_pages(1);
    for (unsigned i = 0; i < n; i++) {
        entries[i].tag = tags[i];
        entries[i].val = (uint64_t)(i + 1) << 12;
    }

    vring_raw_set_desc(&sq, 0, vv_virt_to_phys(entries),
                       n * sizeof(struct virtio_balloon_stat), 0, 0);
    vring_raw_set_avail(&sq, 0, 0);
    vring_raw_set_avail_idx(&sq, 1);

    return vv_kick_and_wait(dev, &sq, 1, VV_TIMEOUT_MS);
}

REGISTER_TEST_REQUIRES(L0043, VIRTIO_PCI_DEVICE_BALLOON,
              test_balloon_stats_remaining_tags,
              "Stats queue accepts the remaining well-known tags",
              VIRTIO_SPEC_V1_2, "5.5.6.2",
              (1ULL << VIRTIO_BALLOON_F_STATS_VQ), 0);
