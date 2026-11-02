/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0151: TX with header only, zero payload.
 *
 * Spec 5.1.6.2: The driver prepends virtio_net_hdr. Submit a frame
 * with only the virtio header and zero bytes of Ethernet data. The
 * device should handle the zero length payload gracefully.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_tx_hdr_only(struct virtio_dev *dev,
                                          struct vring *vr)
{
    struct virtio_net_hdr *hdr = vv_alloc_pages(1);

    hdr->flags = 0;
    hdr->gso_type = VIRTIO_NET_HDR_GSO_NONE;
    hdr->hdr_len = 0;
    hdr->gso_size = 0;
    hdr->csum_start = 0;
    hdr->csum_offset = 0;

    /* Single descriptor: just the virtio header, no Ethernet frame */
    vv_desc(vr, 0, hdr, sizeof(*hdr), 0, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(N0151, VIRTIO_PCI_DEVICE_NET, test_net_tx_hdr_only,
              "TX with header only and zero payload",
              VIRTIO_SPEC_V1_2, "5.1.6.2");
