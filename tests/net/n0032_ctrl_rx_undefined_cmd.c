/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0032: net_ctrl_rx_undefined_cmd
 *
 * Send a control queue command with class=VIRTIO_NET_CTRL_RX but an
 * undefined command ID. Tests that the device rejects unknown commands
 * within a valid class without crashing.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_ctrl_rx_bad_cmd(struct virtio_dev *dev,
                                              struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_NET_F_CTRL_VQ) ||
        !virtio_pci_feature_offered(dev, VIRTIO_NET_F_CTRL_RX))
        return TEST_SKIP;

    struct virtio_net_ctrl_hdr *hdr = vv_alloc_pages(1);
    uint8_t *ack = vv_alloc_pages(1);

    hdr->class = VIRTIO_NET_CTRL_RX;
    hdr->command = 0xFF; /* undefined command within RX class */
    *ack = 0xFF;

    /* ctrl hdr (readable) -> ack (writable) */
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, ack, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_Q_REQUIRES(N0032, VIRTIO_PCI_DEVICE_NET, test_net_ctrl_rx_bad_cmd,
              "CTRL_RX with undefined command ID (0xFF)",
              VIRTIO_SPEC_V1_2, "5.1.6.5.1", VV_QUEUE_LAST,
              (1ULL << VIRTIO_NET_F_CTRL_VQ) |
              (1ULL << VIRTIO_NET_F_CTRL_RX), 0);
