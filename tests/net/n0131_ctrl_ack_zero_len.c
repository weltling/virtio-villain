/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0131: ctrl_ack_zero_len
 *
 * Send a valid control command but with the ack descriptor having
 * zero length. Spec 5.1.6.5 says the device writes a 1-byte ack.
 * With zero length the device has nowhere to write the ack. It must
 * not write beyond the descriptor length and must not crash.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_ctrl_ack_zero_len(struct virtio_dev *dev,
                                                struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_NET_F_CTRL_VQ))
        return TEST_SKIP;

    struct virtio_net_ctrl_hdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *ack = vv_alloc_pages(1);

    hdr->class = VIRTIO_NET_CTRL_RX;
    hdr->command = VIRTIO_NET_CTRL_RX_PROMISC;
    *data = 1;
    *ack = 0xCC; /* canary: should stay unchanged */

    /* hdr (readable) -> data (readable) -> ack (writable, len=0) */
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 1, VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, ack, 0, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_Q_REQUIRES(N0131, VIRTIO_PCI_DEVICE_NET, test_net_ctrl_ack_zero_len,
              "CTRL command with zero length ack descriptor",
              VIRTIO_SPEC_V1_2, "5.1.6.5", VV_QUEUE_LAST,
              (1ULL << VIRTIO_NET_F_CTRL_VQ), 0);
