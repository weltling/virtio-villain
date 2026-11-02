/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0117: TX with NEEDS_CSUM and csum_start past the payload.
 *
 * Spec 5.1.6.2: When VIRTIO_NET_HDR_F_NEEDS_CSUM is set, the
 * device performs the checksum starting at csum_start within
 * the payload. Submit a small payload with csum_start set well
 * beyond the available bytes. The device must reject or drop
 * the frame rather than reading past the descriptor.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_csum_start_past_payload(struct virtio_dev *dev,
                                                      struct vring *vr)
{
    struct virtio_net_hdr_mrg *hdr = vv_alloc_pages(1);
    uint8_t *payload = vv_alloc_pages(1);

    hdr->flags       = VIRTIO_NET_HDR_F_NEEDS_CSUM;
    hdr->gso_type    = VIRTIO_NET_HDR_GSO_NONE;
    hdr->csum_start  = 8000;
    hdr->csum_offset = 0;

    memset(payload, 0xAB, 64);

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, payload, 64, 0, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_Q(N0117, VIRTIO_PCI_DEVICE_NET,
                test_net_csum_start_past_payload,
                "TX NEEDS_CSUM with csum_start beyond payload length",
                VIRTIO_SPEC_V1_2, "5.1.6.2", 1);
