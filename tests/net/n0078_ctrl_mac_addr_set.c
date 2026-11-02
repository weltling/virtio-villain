/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0078: net_ctrl_mac_addr_set
 *
 * Send VIRTIO_NET_CTRL_MAC ADDR_SET command to change the device MAC.
 * Spec 5.1.6.5.2: If VIRTIO_NET_F_CTRL_MAC_ADDR is negotiated, the
 * driver can set the primary MAC address via control virtqueue.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_ctrl_mac_addr_set(struct virtio_dev *dev,
                                                struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_NET_F_CTRL_VQ) ||
        !virtio_pci_feature_offered(dev, VIRTIO_NET_F_CTRL_MAC_ADDR))
        return TEST_SKIP;

    struct vring ctrl_vr;
    vring_alloc(&ctrl_vr, 64);
    vring_attach(dev, &ctrl_vr, 2);

    struct virtio_net_ctrl_hdr *hdr = vv_alloc_pages(1);
    uint8_t *mac = vv_alloc_pages(1);
    uint8_t *ack = vv_alloc_pages(1);

    hdr->class = VIRTIO_NET_CTRL_MAC;
    hdr->command = VIRTIO_NET_CTRL_MAC_ADDR_SET;
    /* Set MAC to 02:00:00:00:00:42 (locally administered unicast) */
    mac[0] = 0x02; mac[1] = 0x00; mac[2] = 0x00;
    mac[3] = 0x00; mac[4] = 0x00; mac[5] = 0x42;
    *ack = 0xFF;

    vv_desc(&ctrl_vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(&ctrl_vr, 1, mac, 6, VRING_DESC_F_NEXT, 2);
    vv_desc(&ctrl_vr, 2, ack, 1, VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(&ctrl_vr, 0, 0);
    vring_raw_set_avail_idx(&ctrl_vr, 1);

    (void)vr;
    return vv_kick_and_wait(dev, &ctrl_vr, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST_REQUIRES(N0078, VIRTIO_PCI_DEVICE_NET, test_net_ctrl_mac_addr_set,
              "Control VQ: set primary MAC address",
              VIRTIO_SPEC_V1_2, "5.1.6.5.2",
              (1ULL << VIRTIO_NET_F_CTRL_VQ) |
              (1ULL << VIRTIO_NET_F_CTRL_MAC_ADDR), 0);
