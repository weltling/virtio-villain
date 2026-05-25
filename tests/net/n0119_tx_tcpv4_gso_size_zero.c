/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0119: TX with GSO_TCPV4 and gso_size = 0.
 *
 * Spec 5.1.6.2.3: gso_size carries the segment size for GSO.
 * Submit a TX packet declaring VIRTIO_NET_HDR_GSO_TCPV4 with
 * gso_size = 0. A zero segment size is invalid and a naive
 * segmenter that divides payload length by gso_size will hit a
 * division by zero. The device must reject the frame rather
 * than crashing.
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

#define VIRTIO_NET_HDR_GSO_TCPV4    1
#define VIRTIO_NET_HDR_F_NEEDS_CSUM 1

static test_result_t test_net_tx_tcpv4_gso_zero(struct virtio_dev *dev,
                                                struct vring *vr)
{
    uint8_t *pkt = vv_alloc_pages(1);
    struct virtio_net_hdr *h = (struct virtio_net_hdr *)pkt;

    memset(pkt, 0, 4096);
    h->flags    = VIRTIO_NET_HDR_F_NEEDS_CSUM;
    h->gso_type = VIRTIO_NET_HDR_GSO_TCPV4;
    h->hdr_len  = 54;
    h->gso_size = 0;
    h->csum_start  = 34;
    h->csum_offset = 16;

    memset(pkt + sizeof(*h), 0xCC, 2048);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(pkt),
                       sizeof(*h) + 2048, 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(N0119, VIRTIO_PCI_DEVICE_NET, test_net_tx_tcpv4_gso_zero,
                "TX GSO_TCPV4 with gso_size = 0",
                VIRTIO_SPEC_V1_2, "5.1.6.2.3", 1);
