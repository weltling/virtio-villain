/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0152: TX 1500 byte frame (typical MTU size).
 *
 * Submit a full MTU sized Ethernet frame (14 byte header + 1486
 * byte payload = 1500 total). Tests that the device handles
 * standard MTU size frames.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_tx_1500(struct virtio_dev *dev, struct vring *vr)
{
    struct virtio_net_hdr *hdr = vv_alloc_pages(1);
    uint8_t *frame = vv_alloc_pages(1);

    hdr->flags = 0;
    hdr->gso_type = VIRTIO_NET_HDR_GSO_NONE;
    hdr->hdr_len = 0; hdr->gso_size = 0;
    hdr->csum_start = 0; hdr->csum_offset = 0;

    /* 1500 byte Ethernet frame */
    memset(frame, 0xFF, 6);
    memset(frame + 6, 0x02, 6);
    frame[12] = 0x08; frame[13] = 0x00;
    memset(frame + 14, 0x42, 1486);

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, frame, 1500, 0, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(N0152, VIRTIO_PCI_DEVICE_NET, test_net_tx_1500,
              "TX 1500 byte frame (standard MTU)",
              VIRTIO_SPEC_V1_2, "5.1.6.2");
