/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0004: net_tx_csum_bad_offset
 *
 * Submit a TX frame where csum_start + csum_offset extends beyond the
 * packet length. The device must not write to memory past the buffer.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_tx_csum_bad_offset(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    struct virtio_net_hdr *hdr = vv_alloc_pages(1);
    uint8_t *frame = vv_alloc_pages(1);

    hdr->flags = VIRTIO_NET_HDR_F_NEEDS_CSUM;
    hdr->gso_type = VIRTIO_NET_HDR_GSO_NONE;
    hdr->hdr_len = 0;
    hdr->gso_size = 0;
    /* csum_start + csum_offset far beyond the 64-byte frame */
    hdr->csum_start = 60000;
    hdr->csum_offset = 5000;

    memset(frame, 0x42, 64);

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, frame, 64, 0, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(N0004, VIRTIO_PCI_DEVICE_NET, test_net_tx_csum_bad_offset,
              "TX with csum_start + csum_offset beyond packet",
              VIRTIO_SPEC_V1_2, "5.1.6");
