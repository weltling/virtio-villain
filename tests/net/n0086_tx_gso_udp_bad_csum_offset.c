/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0086: net_tx_gso_udp_bad_csum_offset
 *
 * Submit a TX descriptor whose virtio_net_hdr declares gso_type
 * UDP yet places csum_offset inside the IP header rather than the
 * UDP header. Spec 5.1.6.2 forbids the combination and the host
 * must drop the packet without crashing.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_tx_gso_udp_bad_csum(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    uint8_t *pkt = vv_alloc_pages(1);
    struct virtio_net_hdr_mrg *h = (void *)pkt;

    h->flags = VIRTIO_NET_HDR_F_NEEDS_CSUM;
    h->gso_type = VIRTIO_NET_HDR_GSO_UDP;
    h->hdr_len = 42;
    h->gso_size = 1400;
    h->csum_start = 14;     /* inside IP header */
    h->csum_offset = 10;    /* inside IP header */
    h->num_buffers = 0;

    /* Fill the rest with a plausible 60 byte ethernet frame */
    memset(pkt + sizeof(*h), 0x55, 60);

    vv_desc(vr, 0, pkt, sizeof(*h) + 60, 0, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_Q(N0086, VIRTIO_PCI_DEVICE_NET, test_net_tx_gso_udp_bad_csum,
                "TX gso_type UDP with csum_offset inside IP header",
                VIRTIO_SPEC_V1_2, "5.1.6.2", 1);
