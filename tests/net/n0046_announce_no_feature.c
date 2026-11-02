/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0046: net_announce_no_feature
 *
 * Write to the control queue with CTRL_ANNOUNCE command without
 * having negotiated VIRTIO_NET_F_GUEST_ANNOUNCE. Tests device
 * handling of unsolicited announce commands.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_announce_no_feature(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_NET_F_CTRL_VQ))
        return TEST_SKIP;

    struct virtio_net_ctrl_hdr *ctrl = vv_alloc_pages(1);
    uint8_t *ack = vv_alloc_pages(1);

    ctrl->class = VIRTIO_NET_CTRL_ANNOUNCE;
    ctrl->command = VIRTIO_NET_CTRL_ANNOUNCE_ACK;
    *ack = 0xFF;

    vv_desc(vr, 0, ctrl, sizeof(*ctrl), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, ack, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_Q_REQUIRES(N0046, VIRTIO_PCI_DEVICE_NET, test_net_announce_no_feature,
              "CTRL_ANNOUNCE without GUEST_ANNOUNCE feature negotiated",
              VIRTIO_SPEC_V1_2, "5.1.6.5", VV_QUEUE_LAST,
              (1ULL << VIRTIO_NET_F_CTRL_VQ), 0);
