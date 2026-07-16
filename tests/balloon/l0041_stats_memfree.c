/* SPDX-License-Identifier: Apache-2.0 */
/*
 * L0041: stats queue submit tag MEMFREE.
 *
 * Spec 5.5.6.2: When VIRTIO_BALLOON_F_STATS_VQ is negotiated
 * the device may request statistics. The driver pushes stat
 * entries as (tag, val) pairs. Submit one MEMFREE stat entry
 * on the stats queue (queue 1) and verify the device consumes it.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_balloon_stat_memfree(struct virtio_dev *dev,
                                               struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_BALLOON_F_STATS_VQ)))
        return TEST_SKIP;

    /* Stats queue is queue 1 (after inflate=0) in legacy layout,
     * but with modern balloon it depends. Check queue exists. */
    cfg->queue_select = 1;
    __sync_synchronize();
    if (cfg->queue_size == 0)
        return TEST_SKIP;

    struct vring sq;
    vring_alloc(&sq, 16);
    vring_attach(dev, &sq, 1);

    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    struct virtio_balloon_stat *entry = vv_alloc_pages(1);
    entry->tag = VIRTIO_BALLOON_S_MEMFREE;
    entry->val = 1024 * 1024;  /* 1MB free, arbitrary */

    vring_raw_set_desc(&sq, 0, vv_virt_to_phys(entry),
                       sizeof(*entry), 0, 0);
    vring_raw_set_avail(&sq, 0, 0);
    vring_raw_set_avail_idx(&sq, 1);

    return vv_kick_and_wait(dev, &sq, 1, VV_TIMEOUT_MS);
}

REGISTER_TEST_REQUIRES(L0041, VIRTIO_PCI_DEVICE_BALLOON,
              test_balloon_stat_memfree,
              "Stats queue submit MEMFREE entry",
              VIRTIO_SPEC_V1_2, "5.5.6.2",
              (1ULL << VIRTIO_BALLOON_F_STATS_VQ), 0);
