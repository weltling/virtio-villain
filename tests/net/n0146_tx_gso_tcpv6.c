/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0146: TX with GSO TCPv6 offload.
 *
 * Spec 5.1.6.2: When VIRTIO_NET_F_HOST_TSO6 is negotiated the
 * driver may set gso_type=VIRTIO_NET_HDR_GSO_TCPV6. Submit a
 * minimal IPv6+TCP frame with TSO parameters.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_tx_gso_tcp6(struct virtio_dev *dev,
                                          struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_NET_F_HOST_TSO6)))
        return TEST_SKIP;
    if (!(cfg->device_feature & (1U << VIRTIO_NET_F_CSUM)))
        return TEST_SKIP;

    struct virtio_net_hdr *hdr = vv_alloc_pages(1);
    uint8_t *frame = vv_alloc_pages(1);

    hdr->flags = VIRTIO_NET_HDR_F_NEEDS_CSUM;
    hdr->gso_type = VIRTIO_NET_HDR_GSO_TCPV6;
    hdr->hdr_len = 74;    /* eth(14) + ipv6(40) + tcp(20) */
    hdr->gso_size = 1440;
    hdr->csum_start = 54; /* TCP header start (14+40) */
    hdr->csum_offset = 16;

    memset(frame, 0, 200);
    memset(frame, 0xFF, 6);
    memset(frame + 6, 0x02, 6);
    frame[12] = 0x86; frame[13] = 0xDD; /* IPv6 */
    frame[14] = 0x60;                    /* version 6 */
    frame[18] = 0x00; frame[19] = 120;  /* payload length */
    frame[20] = 6;                       /* next header = TCP */
    frame[21] = 64;                      /* hop limit */

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(frame), 174, 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_REQUIRES(N0146, VIRTIO_PCI_DEVICE_NET, test_net_tx_gso_tcp6,
              "TX with GSO TCPv6 offload parameters",
              VIRTIO_SPEC_V1_2, "5.1.6.2",
              (1ULL << VIRTIO_NET_F_HOST_TSO6) |
              (1ULL << VIRTIO_NET_F_CSUM), 0);
