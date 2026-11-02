/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0157: TX 512 byte frame.
 *
 * Submit a 512 byte Ethernet frame (14 header + 498 payload).
 * Tests a medium sized frame between minimum and MTU.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_tx_512(struct virtio_dev *dev, struct vring *vr)
{
    struct virtio_net_hdr *hdr = vv_alloc_pages(1);
    uint8_t *frame = vv_alloc_pages(1);

    hdr->flags = 0;
    hdr->gso_type = VIRTIO_NET_HDR_GSO_NONE;
    hdr->hdr_len = 0; hdr->gso_size = 0;
    hdr->csum_start = 0; hdr->csum_offset = 0;

    memset(frame, 0xFF, 6); memset(frame + 6, 0x02, 6);
    frame[12] = 0x08; frame[13] = 0x00;
    memset(frame + 14, 0x55, 498);

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, frame, 512, 0, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(N0157, VIRTIO_PCI_DEVICE_NET, test_net_tx_512,
              "TX 512 byte frame",
              VIRTIO_SPEC_V1_2, "5.1.6.2");
