/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0021: net_tx_num_buffers_nonzero
 *
 * Submit a TX packet with the num_buffers field set to a non-zero value.
 * The num_buffers field is device-only (used in RX with mergeable buffers);
 * the driver must not set it on TX. A VMM that trusts this field on TX
 * may misinterpret the packet boundaries.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_net_hdr_mrg {
    uint8_t  flags;
    uint8_t  gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
    uint16_t num_buffers;
} __attribute__((packed));

#define VIRTIO_NET_HDR_GSO_NONE 0

static test_result_t test_net_tx_num_buffers(struct virtio_dev *dev,
                                             struct vring *vr)
{
    struct virtio_net_hdr_mrg *hdr = vv_alloc_pages(1);
    uint8_t *frame = vv_alloc_pages(1);

    hdr->flags = 0;
    hdr->gso_type = VIRTIO_NET_HDR_GSO_NONE;
    hdr->hdr_len = 0;
    hdr->gso_size = 0;
    hdr->csum_start = 0;
    hdr->csum_offset = 0;
    hdr->num_buffers = 42; /* Non-zero - driver must not set this */

    /* Minimal valid ethernet frame */
    memset(frame, 0xFF, 6);  /* dst MAC broadcast */
    memset(frame + 6, 0x02, 6); /* src MAC */
    frame[12] = 0x08; frame[13] = 0x00; /* EtherType: IPv4 */
    memset(frame + 14, 0, 46); /* payload to reach min frame size */

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t frame_phys = vv_virt_to_phys(frame);

    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, frame_phys, 60, 0, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(N0021, VIRTIO_PCI_DEVICE_NET, test_net_tx_num_buffers,
              "TX with num_buffers field set to non-zero",
              VIRTIO_SPEC_V1_2, "5.1.6.2");
