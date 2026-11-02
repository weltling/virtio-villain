/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0060: CTRL_GUEST_OFFLOADS without feature (spec 5.1.6.5.6)
 *
 * Issue a CTRL_GUEST_OFFLOADS_SET command without negotiating
 * VIRTIO_NET_F_CTRL_GUEST_OFFLOADS.
 */
#include "lib/virtio_test.h"

static test_result_t test_ctrl_guest_offloads(struct virtio_dev *dev,
                                              struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_NET_F_CTRL_VQ))
        return TEST_SKIP;

    struct virtio_net_ctrl_hdr *ctrl = vv_alloc_pages(1);
    uint64_t *offloads = vv_alloc_pages(1);
    uint8_t *ack = vv_alloc_pages(1);

    ctrl->class = VIRTIO_NET_CTRL_GUEST_OFFLOADS;
    ctrl->command = VIRTIO_NET_CTRL_GUEST_OFFLOADS_SET;
    *offloads = 0xFFFFFFFFFFFFFFFFULL; /* all offloads */
    *ack = 0xFF;

    vv_desc(vr, 0, ctrl, sizeof(*ctrl), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, offloads, sizeof(*offloads), VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, ack, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_Q_REQUIRES(N0060, VIRTIO_PCI_DEVICE_NET, test_ctrl_guest_offloads,
              "CTRL_GUEST_OFFLOADS_SET without feature negotiated",
              VIRTIO_SPEC_V1_2, "5.1.6.5.6", VV_QUEUE_LAST,
              (1ULL << VIRTIO_NET_F_CTRL_VQ), 0);
