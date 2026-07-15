/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0144: TX with GSO TCPv4 offload.
 *
 * Spec 5.1.6.2: When VIRTIO_NET_F_HOST_TSO4 is negotiated the
 * driver may set gso_type=VIRTIO_NET_HDR_GSO_TCPV4 with valid
 * hdr_len and gso_size. Submit a frame with TCP segmentation
 * offload parameters. The device must consume it.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_tx_gso_tcp4(struct virtio_dev *dev,
                                          struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_NET_F_HOST_TSO4)))
        return TEST_SKIP;
    if (!(cfg->device_feature & (1U << VIRTIO_NET_F_CSUM)))
        return TEST_SKIP;

    struct virtio_net_hdr *hdr = vv_alloc_pages(1);
    uint8_t *frame = vv_alloc_pages(1);

    hdr->flags = VIRTIO_NET_HDR_F_NEEDS_CSUM;
    hdr->gso_type = VIRTIO_NET_HDR_GSO_TCPV4;
    hdr->hdr_len = 54;    /* eth(14) + ip(20) + tcp(20) */
    hdr->gso_size = 1460; /* typical MSS */
    hdr->csum_start = 34; /* TCP header start */
    hdr->csum_offset = 16; /* TCP checksum offset */

    /* Build a minimal frame: eth + ip + tcp + 100 bytes payload */
    memset(frame, 0, 200);
    memset(frame, 0xFF, 6);         /* dst */
    memset(frame + 6, 0x02, 6);     /* src */
    frame[12] = 0x08; frame[13] = 0x00; /* IPv4 */
    frame[14] = 0x45;               /* IP version + IHL */
    frame[16] = 0x00; frame[17] = 140; /* total length */
    frame[23] = 6;                  /* protocol = TCP */

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(frame), 154, 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_REQUIRES(N0144, VIRTIO_PCI_DEVICE_NET, test_net_tx_gso_tcp4,
              "TX with GSO TCPv4 offload parameters",
              VIRTIO_SPEC_V1_2, "5.1.6.2",
              (1ULL << VIRTIO_NET_F_HOST_TSO4) |
              (1ULL << VIRTIO_NET_F_CSUM), 0);
