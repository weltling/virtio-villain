/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0074: net_ctrl_rx_allmulti
 *
 * Send VIRTIO_NET_CTRL_RX ALLMULTI command via the control virtqueue.
 * Spec 5.1.6.5.1: Setting allmulti causes the device to receive all
 * multicast packets.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_ctrl_rx_allmulti(struct virtio_dev *dev,
                                              struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_NET_F_CTRL_VQ) ||
        !virtio_pci_feature_offered(dev, VIRTIO_NET_F_CTRL_RX))
        return TEST_SKIP;

    struct vring ctrl_vr;
    vring_alloc(&ctrl_vr, 64);
    vring_attach(dev, &ctrl_vr, 2);

    struct virtio_net_ctrl_hdr *hdr = vv_alloc_pages(1);
    uint8_t *on_off = vv_alloc_pages(1);
    uint8_t *ack = vv_alloc_pages(1);

    hdr->class = VIRTIO_NET_CTRL_RX;
    hdr->command = VIRTIO_NET_CTRL_RX_ALLMULTI;
    *on_off = 1;  /* enable allmulti */
    *ack = 0xFF;

    vv_desc(&ctrl_vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(&ctrl_vr, 1, on_off, 1, VRING_DESC_F_NEXT, 2);
    vv_desc(&ctrl_vr, 2, ack, 1, VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(&ctrl_vr, 0, 0);
    vring_raw_set_avail_idx(&ctrl_vr, 1);

    (void)vr;
    return vv_kick_and_wait(dev, &ctrl_vr, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST_REQUIRES(N0074, VIRTIO_PCI_DEVICE_NET, test_net_ctrl_rx_allmulti,
              "Control VQ: enable all-multicast mode",
              VIRTIO_SPEC_V1_2, "5.1.6.5.1",
              (1ULL << VIRTIO_NET_F_CTRL_VQ) |
              (1ULL << VIRTIO_NET_F_CTRL_RX), 0);
