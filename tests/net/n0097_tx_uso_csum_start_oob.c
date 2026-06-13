/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0097: net_tx_uso_csum_start_oob
 *
 * Submit a USO TX packet where csum_start exceeds the data length.
 * Spec v1.3 5.1.6.2: csum_start must not exceed the packet length.
 * Device should detect and reject this malformed packet.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_tx_uso_csum_oob(struct virtio_dev *dev,
                                              struct vring *vr)
{
    uint8_t *pkt = vv_alloc_pages(1);
    struct virtio_net_hdr_mrg *h = (void *)pkt;
    uint16_t payload_len = 60;

    h->flags = VIRTIO_NET_HDR_F_NEEDS_CSUM;
    h->gso_type = VIRTIO_NET_HDR_GSO_UDP_L4;
    h->hdr_len = 42;
    h->gso_size = 1400;
    /* csum_start beyond the total data length */
    h->csum_start = sizeof(*h) + payload_len + 100;
    h->csum_offset = 6;
    h->num_buffers = 0;

    memset(pkt + sizeof(*h), 0xBB, payload_len);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(pkt),
                       sizeof(*h) + payload_len, 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(N0097, VIRTIO_PCI_DEVICE_NET, test_net_tx_uso_csum_oob,
                "TX USO with csum_start beyond packet length",
                VIRTIO_SPEC_V1_3, "5.1.6.2", 1);
