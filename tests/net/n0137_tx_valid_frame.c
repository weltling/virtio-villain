/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0137: TX a valid minimal Ethernet frame.
 *
 * Spec 5.1.6.2: The driver prepends a virtio_net_hdr to every TX
 * buffer. Submit a correctly formed minimal frame (14 byte Ethernet
 * header + 46 byte padding = 60 bytes) with a clean virtio_net_hdr
 * (flags=0, gso_type=NONE). The device must consume it without error.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_tx_valid(struct virtio_dev *dev,
                                       struct vring *vr)
{
    struct virtio_net_hdr *hdr = vv_alloc_pages(1);
    uint8_t *frame = vv_alloc_pages(1);

    /* Clean header: no offloads, no GSO */
    hdr->flags = 0;
    hdr->gso_type = VIRTIO_NET_HDR_GSO_NONE;
    hdr->hdr_len = 0;
    hdr->gso_size = 0;
    hdr->csum_start = 0;
    hdr->csum_offset = 0;

    /* Minimal valid Ethernet frame */
    memset(frame, 0xFF, 6);         /* dst: broadcast */
    memset(frame + 6, 0x02, 6);     /* src: locally administered */
    frame[12] = 0x08; frame[13] = 0x00;  /* EtherType: IPv4 */
    memset(frame + 14, 0x00, 46);   /* padding to minimum 60 bytes */

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, frame, 60, 0, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(N0137, VIRTIO_PCI_DEVICE_NET, test_net_tx_valid,
              "TX a valid minimal Ethernet frame",
              VIRTIO_SPEC_V1_2, "5.1.6.2");
