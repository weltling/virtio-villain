/* SPDX-License-Identifier: Apache-2.0 */
/*
 * L0032: balloon stats reply with an unknown tag value.
 *
 * Spec 5.5.6.1: Each stats entry has a 16 bit tag drawn from the
 * VIRTIO_BALLOON_S_* enumeration. Submit a stats buffer where
 * every entry uses tag=0xFFFF, a value well past any defined
 * tag. The device must accept or drop the reply gracefully
 * without indexing past an internal tag table.
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

static test_result_t test_balloon_stats_unknown_tag(struct virtio_dev *dev,
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

    struct vring svr;
    vring_alloc(&svr, 16);
    vring_attach(dev, &svr, 2);

    struct virtio_balloon_stat *stats = vv_alloc_pages(1);
    for (int i = 0; i < 8; i++) {
        stats[i].tag = 0xFFFF;
        stats[i].val = (uint64_t)i;
    }

    vring_raw_set_desc(&svr, 0, vv_virt_to_phys(stats),
                       (uint32_t)(sizeof(*stats) * 8), 0, 0);
    vring_raw_set_avail(&svr, 0, 0);
    vring_raw_set_avail_idx(&svr, 1);

    return vv_kick_and_wait(dev, &svr, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST(L0032, VIRTIO_PCI_DEVICE_BALLOON,
              test_balloon_stats_unknown_tag,
              "Balloon stats reply with all entries tagged 0xFFFF",
              VIRTIO_SPEC_V1_2, "5.5.6.1");
