/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0007: net_gso_without_csum
 *
 * Set gso_type to TCPV4 without having negotiated VIRTIO_NET_F_CSUM.
 * Spec 5.1.6.2.1: driver MUST NOT set gso_type != NONE unless CSUM
 * is negotiated. The device must reject or ignore.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_gso_without_csum(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_net_hdr *hdr = vv_alloc_pages(1);
    uint8_t *frame = vv_alloc_pages(1);

    /* GSO type set without CSUM feature negotiated */
    hdr->flags = 0;
    hdr->gso_type = VIRTIO_NET_HDR_GSO_TCPV4;
    hdr->hdr_len = 54;
    hdr->gso_size = 1460;
    hdr->csum_start = 34;
    hdr->csum_offset = 16;

    memset(frame, 0x42, 128);

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, frame, 128, 0, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(N0007, VIRTIO_PCI_DEVICE_NET, test_net_gso_without_csum,
              "GSO type set without CSUM feature negotiated",
              VIRTIO_SPEC_V1_2, "5.1.6.2");
