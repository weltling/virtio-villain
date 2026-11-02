/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0030: net_tx_hdr_len_zero
 *
 * Send a TX packet with hdr_len=0 and GSO enabled (TCPv4). A device
 * that uses hdr_len for buffer splitting may divide by zero or
 * miscalculate segment boundaries.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_tx_hdr_len_zero(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_net_hdr *hdr = vv_alloc_pages(1);
    uint8_t *frame = vv_alloc_pages(1);

    hdr->flags = VIRTIO_NET_HDR_F_NEEDS_CSUM;
    hdr->gso_type = VIRTIO_NET_HDR_GSO_TCPV4;
    hdr->hdr_len = 0; /* invalid: should be >= eth+ip+tcp */
    hdr->gso_size = 1500;
    hdr->csum_start = 14;
    hdr->csum_offset = 16;

    memset(frame, 0x42, 128);

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, frame, 128, 0, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(N0030, VIRTIO_PCI_DEVICE_NET, test_net_tx_hdr_len_zero,
              "TX with hdr_len=0 and GSO enabled",
              VIRTIO_SPEC_V1_2, "5.1.6.2");
