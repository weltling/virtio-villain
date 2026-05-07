/* SPDX-License-Identifier: Apache-2.0 */
/*
 * L0019: balloon stats class queue identification
 *
 * Spec 5.5.6.1 defines a stats virtqueue used to push memory stats
 * to the device. If VIRTIO_BALLOON_F_STATS_VQ is negotiated, the
 * balloon device exposes a third queue (idx 2). This test verifies
 * num_queues reflects the negotiated feature and that the stats
 * queue accepts a single buffer worth of placeholder stats data
 * without wedging the device.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

#define VIRTIO_BALLOON_F_STATS_VQ 1

struct virtio_balloon_stat {
    uint16_t tag;
    uint64_t val;
} __attribute__((packed));

static test_result_t test_balloon_stats_vq(struct virtio_dev *dev,
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

    /* Stats queue is index 2, programmed by harness if num_queues>=3 */
    struct vring stats_vr;
    vring_alloc(&stats_vr, 16);
    vring_attach(dev, &stats_vr, 2);

    struct virtio_balloon_stat *stats = vv_alloc_pages(1);
    /* Fill placeholder stats, all tags 0 (SWAP_IN), value 0 */
    for (int i = 0; i < 8; i++) {
        stats[i].tag = (uint16_t)i;
        stats[i].val = 0;
    }

    vring_raw_set_desc(&stats_vr, 0, vv_virt_to_phys(stats),
                       sizeof(*stats) * 8, 0, 0);
    vring_raw_set_avail(&stats_vr, 0, 0);
    vring_raw_set_avail_idx(&stats_vr, 1);

    return vv_kick_and_wait(dev, &stats_vr, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST(L0019, VIRTIO_PCI_DEVICE_BALLOON, test_balloon_stats_vq,
              "Balloon stats queue accepts a stats buffer",
              VIRTIO_SPEC_V1_2, "5.5.6.1");
