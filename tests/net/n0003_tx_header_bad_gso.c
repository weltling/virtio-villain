/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0003: net_tx_header_bad_gso
 *
 * Submit a TX frame with GSO type set but gso_size = 0.
 * The device must reject rather than dividing by zero or producing
 * malformed packets.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_tx_bad_gso(struct virtio_dev *dev,
                                         struct vring *vr)
{
    struct virtio_net_hdr *hdr = vv_alloc_pages(1);
    uint8_t *frame = vv_alloc_pages(1);

    hdr->flags = 0;
    hdr->gso_type = VIRTIO_NET_HDR_GSO_TCPV4;
    hdr->hdr_len = 54; /* typical TCP/IP header */
    hdr->gso_size = 0; /* invalid: zero GSO segment size */
    hdr->csum_start = 34;
    hdr->csum_offset = 16;

    /* Minimal frame payload */
    memset(frame, 0x42, 128);

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t frame_phys = vv_virt_to_phys(frame);

    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, frame_phys, 128, 0, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(N0003, VIRTIO_PCI_DEVICE_NET, test_net_tx_bad_gso,
              "TX with GSO type set but gso_size = 0",
              VIRTIO_SPEC_V1_2, "5.1.6");
