/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0040: net_tx_frame_exactly_mtu
 *
 * Submit a TX frame with size exactly equal to MTU (1500 for default
 * ethernet). This is the boundary case - N17 tests exceeding MTU.
 * The device should accept this frame without error.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_tx_exactly_mtu(struct virtio_dev *dev,
                                             struct vring *vr)
{
    struct virtio_net_hdr *hdr = vv_alloc_pages(1);
    /* Ethernet header (14) + payload = MTU (1500) total = 1514 bytes */
    uint8_t *frame = vv_alloc_pages(1); /* 4096 > 1514 */

    hdr->flags = 0;
    hdr->gso_type = VIRTIO_NET_HDR_GSO_NONE;
    hdr->hdr_len = 0;
    hdr->gso_size = 0;
    hdr->csum_start = 0;
    hdr->csum_offset = 0;

    /* Build ethernet frame exactly 1514 bytes (14 header + 1500 payload) */
    memset(frame, 0xFF, 6);       /* dst MAC broadcast */
    memset(frame + 6, 0x02, 6);   /* src MAC */
    frame[12] = 0x08; frame[13] = 0x00; /* EtherType: IPv4 */
    memset(frame + 14, 0x42, 1500); /* payload = MTU */

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t frame_phys = vv_virt_to_phys(frame);

    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, frame_phys, 1514, 0, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(N0040, VIRTIO_PCI_DEVICE_NET, test_net_tx_exactly_mtu,
              "TX frame exactly MTU size (1500 byte payload)",
              VIRTIO_SPEC_V1_2, "5.1.6.2");
