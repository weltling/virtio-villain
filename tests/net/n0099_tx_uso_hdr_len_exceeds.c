/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0099: net_tx_uso_hdr_len_exceeds_pkt
 *
 * Submit a USO TX packet where hdr_len exceeds the total buffer
 * length. Spec v1.3 5.1.6.2: device must handle this gracefully.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_tx_uso_hdr_too_big(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    uint8_t *pkt = vv_alloc_pages(1);
    struct virtio_net_hdr_mrg *h = (void *)pkt;
    uint16_t payload_len = 60;

    h->flags = 0;
    h->gso_type = VIRTIO_NET_HDR_GSO_UDP_L4;
    /* hdr_len larger than the entire descriptor */
    h->hdr_len = sizeof(*h) + payload_len + 500;
    h->gso_size = 1400;
    h->csum_start = 0;
    h->csum_offset = 0;
    h->num_buffers = 0;

    memset(pkt + sizeof(*h), 0xDD, payload_len);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(pkt),
                       sizeof(*h) + payload_len, 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(N0099, VIRTIO_PCI_DEVICE_NET, test_net_tx_uso_hdr_too_big,
                "TX USO with hdr_len exceeding packet length",
                VIRTIO_SPEC_V1_3, "5.1.6.2", 1);
