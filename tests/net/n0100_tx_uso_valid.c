/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0100: net_tx_uso_valid
 *
 * Submit a well formed TX packet with USOv4 gso_type. This is a
 * positive path test exercising USO segmentation offload as specified
 * in v1.3 5.1.6.2.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_tx_uso_valid(struct virtio_dev *dev,
                                           struct vring *vr)
{
    uint8_t *pkt = vv_alloc_pages(1);
    struct virtio_net_hdr_mrg *h = (void *)pkt;
    uint16_t payload_len = 200;

    h->flags = VIRTIO_NET_HDR_F_NEEDS_CSUM;
    h->gso_type = VIRTIO_NET_HDR_GSO_UDP_L4;
    h->hdr_len = 42;  /* Ethernet(14) + IPv4(20) + UDP(8) */
    h->gso_size = 64;
    h->csum_start = 34;  /* Start of UDP header */
    h->csum_offset = 6;  /* UDP checksum field offset */
    h->num_buffers = 0;

    /* Fill with recognizable payload */
    memset(pkt + sizeof(*h), 0xAA, payload_len);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(pkt),
                       sizeof(*h) + payload_len, 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(N0100, VIRTIO_PCI_DEVICE_NET, test_net_tx_uso_valid,
                "TX USO with valid USOv4 parameters",
                VIRTIO_SPEC_V1_3, "5.1.6.2", 1);
