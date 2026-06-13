/* SPDX-License-Identifier: Apache-2.0 */
/*
 * L0030: balloon stats response split across four descriptors.
 *
 * Spec 5.5.6.1: the stats buffer is a sequence of virtio_balloon_stat
 * entries. Submit one logical buffer split across four chained
 * descriptors, each holding two entries. The device must accept
 * the scattered buffer as a single contiguous stats payload.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_balloon_stats_split(struct virtio_dev *dev,
                                              struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_BALLOON_F_STATS_VQ)))
        return TEST_SKIP;
    if (cfg->num_queues < 3)
        return TEST_SKIP;

    struct vring sq;
    vring_alloc(&sq, 16);
    vring_attach(dev, &sq, 2);

    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    struct virtio_balloon_stat *stats = vv_alloc_pages(1);
    for (int i = 0; i < 8; i++) {
        stats[i].tag = (uint16_t)i;
        stats[i].val = 0;
    }

    uint64_t base = vv_virt_to_phys(stats);
    uint32_t seg = (uint32_t)(sizeof(*stats) * 2);

    vring_raw_set_desc(&sq, 0, base + seg * 0, seg,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&sq, 1, base + seg * 1, seg,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(&sq, 2, base + seg * 2, seg,
                       VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(&sq, 3, base + seg * 3, seg, 0, 0);

    vring_raw_set_avail(&sq, 0, 0);
    vring_raw_set_avail_idx(&sq, 1);

    return vv_kick_and_wait(dev, &sq, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST(L0030, VIRTIO_PCI_DEVICE_BALLOON,
              test_balloon_stats_split,
              "Stats buffer split across four chained descriptors",
              VIRTIO_SPEC_V1_2, "5.5.6.1");
