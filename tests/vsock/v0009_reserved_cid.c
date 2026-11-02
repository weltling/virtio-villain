/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0009: vsock_reserved_cid
 *
 * Use reserved CID values (0, 1, 2, 0xFFFFFFFF) as dst_cid.
 * CID 0 is reserved, 1 is reserved, 2 is host, 0xFFFFFFFF is any.
 * Sending to these may confuse VMM routing logic.
 */
#include "lib/virtio_test.h"

static test_result_t test_vsock_reserved_cid(struct virtio_dev *dev,
                                             struct vring *vr)
{
    struct virtio_vsock_hdr *pkt = vv_alloc_pages(1);
    uint8_t *payload = vv_alloc_pages(1);

    memset(payload, 0xCC, 16);

    /* Try dst_cid = 0xFFFFFFFF (VMADDR_CID_ANY) */
    pkt->src_cid = 3;
    pkt->dst_cid = 0xFFFFFFFF;
    pkt->src_port = 1234;
    pkt->dst_port = 5678;
    pkt->len = 16;
    pkt->type = VIRTIO_VSOCK_TYPE_STREAM;
    pkt->op = VIRTIO_VSOCK_OP_RW;
    pkt->flags = 0;
    pkt->buf_alloc = 4096;
    pkt->fwd_cnt = 0;

    vv_desc(vr, 0, pkt, sizeof(*pkt), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, payload, 16, 0, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(V0009, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_reserved_cid,
              "Packet with reserved CID (0xFFFFFFFF)",
              VIRTIO_SPEC_V1_2, "5.10.6");
