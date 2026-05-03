/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0055: TX with ECN flag without ECN feature (spec 5.1.6.2)
 *
 * Set gso_type to GSO_TCPV4_ECN (3) in the TX header without
 * negotiating VIRTIO_NET_F_HOST_ECN. The device should reject.
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
#define VIRTIO_NET_HDR_GSO_TCPV4_ECN 3

static test_result_t test_net_tx_ecn_no_feature(struct virtio_dev *dev,
                                                struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    if (cfg->num_queues < 2)
        return TEST_SKIP;

    struct vring txvr;
    vring_alloc(&txvr, 16);
    vring_attach(dev, &txvr, 1);

    struct virtio_net_hdr *hdr = vv_alloc_pages(1);
    uint8_t *payload = vv_alloc_pages(1);

    memset(hdr, 0, sizeof(*hdr));
    hdr->flags = VIRTIO_NET_HDR_F_NEEDS_CSUM;
    hdr->gso_type = VIRTIO_NET_HDR_GSO_TCPV4_ECN;
    hdr->hdr_len = 54;
    hdr->gso_size = 1460;
    hdr->csum_start = 34;
    hdr->csum_offset = 16;

    /* Build a 128-byte "packet" */
    memset(payload, 0xBB, 128);

    vring_raw_set_desc(&txvr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&txvr, 1, vv_virt_to_phys(payload), 128, 0, 0);
    vring_raw_set_avail(&txvr, 0, 0);
    vring_raw_set_avail_idx(&txvr, 1);

    return vv_kick_and_wait(dev, &txvr, 1, VV_TIMEOUT_MS);
}

REGISTER_TEST(N0055, VIRTIO_PCI_DEVICE_NET, test_net_tx_ecn_no_feature,
              "TX GSO_TCPV4_ECN without HOST_ECN feature",
              VIRTIO_SPEC_V1_2, "5.1.6.2");
