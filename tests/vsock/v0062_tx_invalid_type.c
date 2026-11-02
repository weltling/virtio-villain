/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0062: Vsock TX with invalid type field.
 *
 * Spec 5.10.6: Submit a vsock packet with type set to 0xFFFF
 * (undefined transport operation). The device must reject the
 * packet rather than misinterpreting the control field.
 */
#include "lib/virtio_test.h"

static test_result_t test_vsock_tx_invalid_type(struct virtio_dev *dev,
                                                struct vring *vr)
{
    struct virtio_vsock_hdr *pkt = vv_alloc_pages(1);

    /* Read guest CID from config */
    volatile uint64_t *cfg = (volatile uint64_t *)dev->device_cfg;
    uint64_t guest_cid = *cfg;

    pkt->src_cid = guest_cid;
    pkt->dst_cid = 2; /* host */
    pkt->src_port = 1234;
    pkt->dst_port = 5678;
    pkt->len = 0;
    pkt->type = 0xFFFF; /* invalid */
    pkt->op = VIRTIO_VSOCK_OP_REQUEST;

    vv_desc(vr, 0, pkt, sizeof(*pkt), 0, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_Q(V0062, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_tx_invalid_type,
                "Vsock TX with invalid type field",
                VIRTIO_SPEC_V1_2, "5.10.6", 1);
