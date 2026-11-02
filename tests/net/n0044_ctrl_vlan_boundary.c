/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0044: net_ctrl_vlan_max_and_invalid
 *
 * Send CTRL_VLAN ADD with vid=4095 (max valid 12-bit VLAN ID) then
 * vid=4096 (invalid, exceeds 12-bit range). Tests device VLAN ID
 * validation at boundaries.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_ctrl_vlan_boundary(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_NET_F_CTRL_VQ) ||
        !virtio_pci_feature_offered(dev, VIRTIO_NET_F_CTRL_VLAN))
        return TEST_SKIP;

    struct virtio_net_ctrl_hdr *ctrl = vv_alloc_pages(1);
    uint16_t *vid = vv_alloc_pages(1);
    uint8_t *ack = vv_alloc_pages(1);

    /* First: vid = 4096 (invalid) */
    ctrl->class = VIRTIO_NET_CTRL_VLAN;
    ctrl->command = VIRTIO_NET_CTRL_VLAN_ADD;
    *vid = 4096; /* exceeds 12-bit max */
    *ack = 0xFF;

    vv_desc(vr, 0, ctrl, sizeof(*ctrl), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, vid, sizeof(*vid), VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, ack, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_Q_REQUIRES(N0044, VIRTIO_PCI_DEVICE_NET, test_net_ctrl_vlan_boundary,
              "CTRL_VLAN ADD with vid=4096 (invalid, exceeds 12 bits)",
              VIRTIO_SPEC_V1_2, "5.1.6.5", VV_QUEUE_LAST,
              (1ULL << VIRTIO_NET_F_CTRL_VQ) |
              (1ULL << VIRTIO_NET_F_CTRL_VLAN), 0);
