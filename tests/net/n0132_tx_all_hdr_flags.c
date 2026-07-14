/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0132: net_tx_all_hdr_flags
 *
 * Transmit a packet with all known header flags set simultaneously
 * (NEEDS_CSUM | DATA_VALID | RSC_INFO). This is an illegal combination
 * per spec 5.1.6.2 since NEEDS_CSUM and DATA_VALID are mutually
 * exclusive. The device must handle it gracefully without crashing.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_tx_all_flags(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_net_hdr *hdr = vv_alloc_pages(1);
    uint8_t *payload = vv_alloc_pages(1);

    /* Set all known flags (mutually exclusive combo) */
    hdr->flags = VIRTIO_NET_HDR_F_NEEDS_CSUM |
                 VIRTIO_NET_HDR_F_DATA_VALID |
                 VIRTIO_NET_HDR_F_RSC_INFO;
    hdr->gso_type = VIRTIO_NET_HDR_GSO_NONE;
    hdr->hdr_len = 0;
    hdr->gso_size = 0;
    hdr->csum_start = 0;
    hdr->csum_offset = 0;

    /* Minimal Ethernet frame */
    memset(payload, 0xFF, 6);  /* dst MAC broadcast */
    memset(payload + 6, 0x01, 6);  /* src MAC */
    payload[12] = 0x08; payload[13] = 0x00; /* EtherType IPv4 */
    memset(payload + 14, 0, 46); /* pad to minimum frame */

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t payload_phys = vv_virt_to_phys(payload);

    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, payload_phys, 60, 0, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(N0132, VIRTIO_PCI_DEVICE_NET, test_net_tx_all_flags,
              "TX with all header flags set (illegal combination)",
              VIRTIO_SPEC_V1_2, "5.1.6.2");
