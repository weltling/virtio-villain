/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0027: net_tx_invalid_gso_type
 *
 * Send a TX packet with an invalid GSO type value (not NONE, TCPv4,
 * UDP, TCPv6, or ECN). The device must not crash on undefined values.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_tx_invalid_gso_type(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    struct virtio_net_hdr *hdr = vv_alloc_pages(1);
    uint8_t *frame = vv_alloc_pages(1);

    hdr->flags = 0;
    hdr->gso_type = 0xFF; /* invalid GSO type */
    hdr->hdr_len = 0;
    hdr->gso_size = 64;
    hdr->csum_start = 0;
    hdr->csum_offset = 0;

    /* Minimal ethernet frame */
    memset(frame, 0x42, 64);

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, frame, 64, 0, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(N0027, VIRTIO_PCI_DEVICE_NET, test_net_tx_invalid_gso_type,
              "TX with invalid GSO type value (0xFF)",
              VIRTIO_SPEC_V1_2, "5.1.6.2");
