/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0088: net_tx_uso4_no_feature
 *
 * Submit a TX packet whose virtio_net_hdr advertises a USOv4
 * gso_type (5). The harness never negotiates GUEST_USO4 or
 * HOST_USO. Spec 5.1.6.2 says the host must reject a USO TX
 * frame in this case without crashing.
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

#define VIRTIO_NET_HDR_GSO_UDP_L4 5

static test_result_t test_net_tx_uso4_no_feature(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    uint8_t *pkt = vv_alloc_pages(1);
    struct virtio_net_hdr *h = (void *)pkt;

    h->flags = 0;
    h->gso_type = VIRTIO_NET_HDR_GSO_UDP_L4;
    h->hdr_len = 42;
    h->gso_size = 1400;
    h->csum_start = 0;
    h->csum_offset = 0;
    h->num_buffers = 0;

    memset(pkt + sizeof(*h), 0x77, 60);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(pkt),
                       sizeof(*h) + 60, 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(N0088, VIRTIO_PCI_DEVICE_NET, test_net_tx_uso4_no_feature,
                "TX with USOv4 gso_type without USO feature negotiated",
                VIRTIO_SPEC_V1_3, "5.1.6.2", 1);
