/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0001: net_tx_oversized
 *
 * Submit a TX frame exceeding the reported MTU. The device should
 * drop or reject the oversized frame rather than crashing.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_tx_oversized(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_net_hdr *hdr = vv_alloc_pages(1);
    /* Allocate a huge frame - 64 KiB (way beyond any MTU) */
    uint8_t *frame = vv_alloc_pages(16);

    hdr->flags = 0;
    hdr->gso_type = VIRTIO_NET_HDR_GSO_NONE;
    hdr->hdr_len = 0;
    hdr->gso_size = 0;
    hdr->csum_start = 0;
    hdr->csum_offset = 0;

    /* Fill with dummy ethernet frame data */
    memset(frame, 0xAA, 65535);

    /* TX queue is typically queue 1 for net devices */
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, frame, 65535, 0, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(N0001, VIRTIO_PCI_DEVICE_NET, test_net_tx_oversized,
              "TX frame exceeding MTU",
              VIRTIO_SPEC_V1_2, "5.1.6");
