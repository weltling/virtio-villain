/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0003: net_tx_header_bad_gso
 *
 * Submit a TX frame with GSO type set but gso_size = 0.
 * The device must reject rather than dividing by zero or producing
 * malformed packets.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_tx_bad_gso(struct virtio_dev *dev,
                                         struct vring *vr)
{
    struct virtio_net_hdr *hdr = vv_alloc_pages(1);
    uint8_t *frame = vv_alloc_pages(1);

    hdr->flags = 0;
    hdr->gso_type = VIRTIO_NET_HDR_GSO_TCPV4;
    hdr->hdr_len = 54; /* typical TCP/IP header */
    hdr->gso_size = 0; /* invalid: zero GSO segment size */
    hdr->csum_start = 34;
    hdr->csum_offset = 16;

    /* Minimal frame payload */
    memset(frame, 0x42, 128);

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, frame, 128, 0, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(N0003, VIRTIO_PCI_DEVICE_NET, test_net_tx_bad_gso,
              "TX with GSO type set but gso_size = 0",
              VIRTIO_SPEC_V1_2, "5.1.6");
