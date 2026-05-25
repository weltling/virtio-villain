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
    uint16_t num_buffers;
} __attribute__((packed));

#define VIRTIO_NET_HDR_F_NEEDS_CSUM 1
#define VIRTIO_NET_HDR_GSO_NONE     0

static test_result_t test_net_csum_start_past_payload(struct virtio_dev *dev,
                                                      struct vring *vr)
{
    struct virtio_net_hdr *hdr = vv_alloc_pages(1);
    uint8_t *payload = vv_alloc_pages(1);

    memset(hdr, 0, sizeof(*hdr));
    hdr->flags       = VIRTIO_NET_HDR_F_NEEDS_CSUM;
    hdr->gso_type    = VIRTIO_NET_HDR_GSO_NONE;
    hdr->csum_start  = 8000;
    hdr->csum_offset = 0;

    memset(payload, 0xAB, 64);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(payload), 64, 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(N0117, VIRTIO_PCI_DEVICE_NET,
                test_net_csum_start_past_payload,
                "TX NEEDS_CSUM with csum_start beyond payload length",
                VIRTIO_SPEC_V1_2, "5.1.6.2", 1);
