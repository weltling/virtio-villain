/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0029: net_tx_csum_start_beyond_packet
 *
 * Send a TX packet with csum_start pointing beyond the packet length.
 * The device must not compute a checksum at an out-of-bounds offset.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_tx_csum_oob(struct virtio_dev *dev,
                                          struct vring *vr)
{
    struct virtio_net_hdr *hdr = vv_alloc_pages(1);
    uint8_t *frame = vv_alloc_pages(1);

    hdr->flags = VIRTIO_NET_HDR_F_NEEDS_CSUM;
    hdr->gso_type = VIRTIO_NET_HDR_GSO_NONE;
    hdr->hdr_len = 0;
    hdr->gso_size = 0;
    hdr->csum_start = 1000; /* way beyond 64-byte frame */
    hdr->csum_offset = 2;

    memset(frame, 0x42, 64);

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, frame, 64, 0, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(N0029, VIRTIO_PCI_DEVICE_NET, test_net_tx_csum_oob,
              "TX with csum_start beyond packet length",
              VIRTIO_SPEC_V1_2, "5.1.6.2");
