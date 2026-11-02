/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0125: DEVICE_STATS control command paths.
 *
 * v1.4 5.1.4 plus VIRTIO_NET_F_DEVICE_STATS (bit 65): driver
 * issues CTRL class VIRTIO_NET_CTRL_STATS with subcommand
 * VIRTIO_NET_CTRL_STATS_GET. If feature not negotiated,
 * skip.
 */
#include "lib/virtio_test.h"

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_NET_F_DEVICE_STATS))
        return TEST_SKIP;

    struct virtio_net_ctrl_hdr *h = vv_alloc_pages(1);
    uint8_t *ack = vv_alloc_pages(1);
    h->class  = VIRTIO_NET_CTRL_STATS;
    h->command = VIRTIO_NET_CTRL_STATS_GET;
    *ack = 0xFF;

    vv_desc(vr, 0, h, sizeof(*h), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, ack, 1, VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(N0125, VIRTIO_PCI_DEVICE_NET, test,
                "DEVICE_STATS GET ctrl command",
                VIRTIO_SPEC_V1_4, "5.1.4", VV_QUEUE_LAST);
