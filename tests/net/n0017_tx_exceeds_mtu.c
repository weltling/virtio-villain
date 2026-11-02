/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0017: net_tx_exceeds_mtu
 *
 * Transmit a frame whose payload exceeds the device MTU. Spec 5.1.4.2:
 * if the driver negotiates VIRTIO_NET_F_MTU, it MUST NOT transmit
 * packets larger than the MTU. The device must handle safely.
 *
 * Since the harness does not negotiate the MTU feature, this
 * test sends an unreasonably large frame that exceeds any sane MTU to
 * exercise the device's size validation path.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_tx_exceeds_mtu(struct virtio_dev *dev,
                                             struct vring *vr)
{
    struct virtio_net_hdr *hdr = vv_alloc_pages(1);
    /* 32 KiB frame - far beyond standard 1500 byte MTU */
    uint8_t *frame = vv_alloc_pages(8);

    hdr->flags = 0;
    hdr->gso_type = VIRTIO_NET_HDR_GSO_NONE;
    hdr->hdr_len = 0;
    hdr->gso_size = 0;
    hdr->csum_start = 0;
    hdr->csum_offset = 0;

    memset(frame, 0xBB, 32768);

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, frame, 32768, 0, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(N0017, VIRTIO_PCI_DEVICE_NET, test_net_tx_exceeds_mtu,
              "TX frame exceeding standard MTU",
              VIRTIO_SPEC_V1_2, "5.1.4");
