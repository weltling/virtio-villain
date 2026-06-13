/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0042: net_tx_gso_short_hdr_len
 *
 * Submit a TX packet with GSO_TCPV4 but hdr_len set smaller than
 * the minimum TCP+IP header size (40 bytes). The device must validate
 * that hdr_len covers the transport headers when GSO is in use.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_tx_gso_short_hdr(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_net_hdr *hdr = vv_alloc_pages(1);
    uint8_t *frame = vv_alloc_pages(1);

    hdr->flags = VIRTIO_NET_HDR_F_NEEDS_CSUM;
    hdr->gso_type = VIRTIO_NET_HDR_GSO_TCPV4;
    hdr->hdr_len = 10; /* Way too short for IP+TCP (minimum 40) */
    hdr->gso_size = 1460;
    hdr->csum_start = 14;   /* after eth header */
    hdr->csum_offset = 16;  /* TCP csum field */

    /* Build a minimal frame */
    memset(frame, 0xFF, 6);       /* dst */
    memset(frame + 6, 0x02, 6);   /* src */
    frame[12] = 0x08; frame[13] = 0x00; /* IPv4 */
    memset(frame + 14, 0x45, 1);  /* IP ver+ihl */
    memset(frame + 15, 0, 2000 - 15);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(frame), 2000, 0, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(N0042, VIRTIO_PCI_DEVICE_NET, test_net_tx_gso_short_hdr,
              "TX GSO_TCPV4 with hdr_len too short for IP+TCP",
              VIRTIO_SPEC_V1_2, "5.1.6.2");
