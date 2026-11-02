/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0111: net_ctrl_announce_without_status
 *
 * Send CTRL_ANNOUNCE ACK without VIRTIO_NET_F_GUEST_ANNOUNCE.
 * Spec 5.1.6.5.2 says announcement requires the feature bit.
 * The device must reject or ignore the command when the feature
 * was not negotiated.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_announce_no_feature(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_NET_F_CTRL_VQ))
        return TEST_SKIP;

    uint8_t *page = vv_alloc_pages(1);

    struct virtio_net_ctrl_hdr *ctrl = (void *)page;
    ctrl->class = VIRTIO_NET_CTRL_ANNOUNCE;
    ctrl->command = VIRTIO_NET_CTRL_ANNOUNCE_ACK;

    uint8_t *status = vv_alloc_pages(1);
    *status = 0xFF;

    vv_desc(vr, 0, page, sizeof(*ctrl), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_Q_REQUIRES(N0111, VIRTIO_PCI_DEVICE_NET, test_net_announce_no_feature,
                "CTRL_ANNOUNCE without guest announce feature",
                VIRTIO_SPEC_V1_2, "5.1.6.5.2", VV_QUEUE_LAST,
                (1ULL << VIRTIO_NET_F_CTRL_VQ), 0);
