/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0004: net_tx_csum_bad_offset
 *
 * Submit a TX frame where csum_start + csum_offset extends beyond the
 * packet length. The device must not write to memory past the buffer.
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

#define VIRTIO_NET_HDR_F_NEEDS_CSUM 1
#define VIRTIO_NET_HDR_GSO_NONE     0

static test_result_t test_net_tx_csum_bad_offset(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    struct virtio_net_hdr *hdr = vv_alloc_pages(1);
    uint8_t *frame = vv_alloc_pages(1);

    hdr->flags = VIRTIO_NET_HDR_F_NEEDS_CSUM;
    hdr->gso_type = VIRTIO_NET_HDR_GSO_NONE;
    hdr->hdr_len = 0;
    hdr->gso_size = 0;
    /* csum_start + csum_offset far beyond the 64-byte frame */
    hdr->csum_start = 60000;
    hdr->csum_offset = 5000;

    memset(frame, 0x42, 64);

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t frame_phys = vv_virt_to_phys(frame);

    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, frame_phys, 64, 0, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(N0004, VIRTIO_PCI_DEVICE_NET, test_net_tx_csum_bad_offset,
              "TX with csum_start + csum_offset beyond packet",
              VIRTIO_SPEC_V1_2, "5.1.6");
