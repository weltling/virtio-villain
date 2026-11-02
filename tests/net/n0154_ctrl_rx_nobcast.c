/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0154: ctrl RX no-broadcast enable.
 *
 * Spec 5.1.6.5.1: Send CTRL_RX_NOBCAST with data=1.
 * Verify VIRTIO_NET_OK.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_ctrl_nobcast(struct virtio_dev *dev,
                                           struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    if (!virtio_pci_feature_offered(dev, VIRTIO_NET_F_CTRL_RX))
        return TEST_SKIP;
    if (!(cfg->device_feature & (1U << VIRTIO_NET_F_CTRL_VQ)))
        return TEST_SKIP;

    struct virtio_net_ctrl_hdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *ack = vv_alloc_pages(1);

    hdr->class = VIRTIO_NET_CTRL_RX;
    hdr->command = VIRTIO_NET_CTRL_RX_NOBCAST;
    *data = 1;
    *ack = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 1, VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, ack, 1, VRING_DESC_F_WRITE, 0);
    test_result_t r = vv_submit_and_wait(dev, vr, 0);
    if (r != TEST_PASS) return r;

    if (*ack != VIRTIO_NET_OK)
        TFAIL("ack %u, expected VIRTIO_NET_OK", *ack);

    return TEST_PASS;
}

REGISTER_TEST_Q_REQUIRES(N0154, VIRTIO_PCI_DEVICE_NET, test_net_ctrl_nobcast,
              "CTRL RX no-broadcast enable",
              VIRTIO_SPEC_V1_2, "5.1.6.5.1", VV_QUEUE_LAST,
              (1ULL << VIRTIO_NET_F_CTRL_RX) |
              (1ULL << VIRTIO_NET_F_CTRL_VQ), 0);
