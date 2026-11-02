/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0098: net_tx_uso_gso_size_zero
 *
 * Submit a USO TX packet with gso_size = 0. Spec v1.3 5.1.6.2:
 * a gso_size of zero for USO is invalid and must not cause a
 * division by zero or infinite loop in the device.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_tx_uso_gso_zero(struct virtio_dev *dev,
                                              struct vring *vr)
{
    uint8_t *pkt = vv_alloc_pages(1);
    struct virtio_net_hdr_mrg *h = (void *)pkt;

    h->flags = 0;
    h->gso_type = VIRTIO_NET_HDR_GSO_UDP_L4;
    h->hdr_len = 42;
    h->gso_size = 0;  /* Invalid: zero segment size */
    h->csum_start = 0;
    h->csum_offset = 0;
    h->num_buffers = 0;

    memset(pkt + sizeof(*h), 0xCC, 60);

    vv_desc(vr, 0, pkt, sizeof(*h) + 60, 0, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_Q(N0098, VIRTIO_PCI_DEVICE_NET, test_net_tx_uso_gso_zero,
                "TX USO with gso_size zero",
                VIRTIO_SPEC_V1_3, "5.1.6.2", 1);
