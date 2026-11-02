/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0008: net_missing_needs_csum
 *
 * Provide a partial checksum without setting VIRTIO_NET_HDR_F_NEEDS_CSUM.
 * Spec 5.1.6.2.1: driver MUST set NEEDS_CSUM when providing partial
 * checksum. Device must handle safely.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_missing_needs_csum(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    struct virtio_net_hdr *hdr = vv_alloc_pages(1);
    uint8_t *frame = vv_alloc_pages(1);

    /*
     * Set csum_start/csum_offset as if partial checksum is valid,
     * but do NOT set NEEDS_CSUM flag (flags = 0).
     */
    hdr->flags = 0;
    hdr->gso_type = VIRTIO_NET_HDR_GSO_NONE;
    hdr->hdr_len = 0;
    hdr->gso_size = 0;
    hdr->csum_start = 34;
    hdr->csum_offset = 16;

    memset(frame, 0x42, 128);

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, frame, 128, 0, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(N0008, VIRTIO_PCI_DEVICE_NET, test_net_missing_needs_csum,
              "Partial checksum without NEEDS_CSUM flag",
              VIRTIO_SPEC_V1_2, "5.1.6.2");
