/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0010: net_tx_writable_desc
 *
 * Place a device-writable descriptor in the TX queue. Spec 5.1.6.4.2:
 * driver MUST NOT place device-writable buffers into the tx queue.
 * The device must reject rather than reading from a write-only buffer.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_tx_writable_desc(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_net_hdr *hdr = vv_alloc_pages(1);
    uint8_t *frame = vv_alloc_pages(1);

    hdr->flags = 0;
    hdr->gso_type = VIRTIO_NET_HDR_GSO_NONE;
    hdr->hdr_len = 0;
    hdr->gso_size = 0;
    hdr->csum_start = 0;
    hdr->csum_offset = 0;

    memset(frame, 0x42, 64);

    /* TX descriptors with WRITE flag - wrong direction */
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_WRITE | VRING_DESC_F_NEXT,
            1);
    vv_desc(vr, 1, frame, 64, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(N0010, VIRTIO_PCI_DEVICE_NET, test_net_tx_writable_desc,
              "Device-writable descriptor in TX queue",
              VIRTIO_SPEC_V1_2, "5.1.6.4");
