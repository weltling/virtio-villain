/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0142: TX with valid NEEDS_CSUM offload header.
 *
 * Spec 5.1.6.2: When VIRTIO_NET_F_CSUM is negotiated, the driver
 * may set NEEDS_CSUM flag with valid csum_start and csum_offset.
 * Submit a valid frame with partial checksum offload and verify
 * the device consumes it.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_tx_csum_valid(struct virtio_dev *dev,
                                            struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_NET_F_CSUM)))
        return TEST_SKIP;

    struct virtio_net_hdr *hdr = vv_alloc_pages(1);
    uint8_t *frame = vv_alloc_pages(1);

    /* Valid NEEDS_CSUM: csum_start at IP header (offset 14),
     * csum_offset at UDP checksum field (offset 6 within UDP = 14+20+6=40) */
    hdr->flags = VIRTIO_NET_HDR_F_NEEDS_CSUM;
    hdr->gso_type = VIRTIO_NET_HDR_GSO_NONE;
    hdr->hdr_len = 0;
    hdr->gso_size = 0;
    hdr->csum_start = 34;   /* start of UDP header (14 eth + 20 ip) */
    hdr->csum_offset = 6;   /* offset of checksum within UDP header */

    /* Build a minimal valid frame */
    memset(frame, 0xFF, 6);         /* dst */
    memset(frame + 6, 0x02, 6);     /* src */
    frame[12] = 0x08; frame[13] = 0x00; /* IPv4 */
    memset(frame + 14, 0, 46);      /* IP + UDP payload */
    /* Minimal IP header */
    frame[14] = 0x45;               /* version + IHL */
    frame[16] = 0x00; frame[17] = 32; /* total length */
    frame[23] = 17;                 /* protocol = UDP */

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(frame), 60, 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_REQUIRES(N0142, VIRTIO_PCI_DEVICE_NET, test_net_tx_csum_valid,
              "TX with valid NEEDS_CSUM offload parameters",
              VIRTIO_SPEC_V1_2, "5.1.6.2",
              (1ULL << VIRTIO_NET_F_CSUM), 0);
