/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0017: net_tx_exceeds_mtu
 *
 * Transmit a frame whose payload exceeds the device MTU. Spec 5.1.4.2:
 * if the driver negotiates VIRTIO_NET_F_MTU, it MUST NOT transmit
 * packets larger than the MTU. The device must handle safely.
 *
 * Since the harness negotiates zero features (no MTU negotiation), this
 * test sends an unreasonably large frame that exceeds any sane MTU to
 * exercise the device's size validation path.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_net_hdr {
    uint8_t  flags;
    uint8_t  gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
} __attribute__((packed));

#define VIRTIO_NET_HDR_GSO_NONE 0

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

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t frame_phys = vv_virt_to_phys(frame);

    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, frame_phys, 32768, 0, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(N0017, VIRTIO_PCI_DEVICE_NET, test_net_tx_exceeds_mtu,
              "TX frame exceeding standard MTU",
              VIRTIO_SPEC_V1_2, "5.1.4");
