/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0056: vsock_no_implied_stream_type_field
 *
 * When VIRTIO_VSOCK_F_NO_IMPLIED_STREAM is active, the type field
 * becomes mandatory in every packet header. Send a packet with
 * type=0 (invalid). Spec v1.3 5.10.6.6.2: without valid type the
 * packet must be rejected.
 */
#include "lib/virtio_test.h"

static test_result_t test_vsock_type_zero(struct virtio_dev *dev,
                                          struct vring *vr)
{
    struct virtio_vsock_hdr *pkt = vv_alloc_pages(1);

    pkt->src_cid = 3;
    pkt->dst_cid = 2;
    pkt->src_port = 1234;
    pkt->dst_port = 5678;
    pkt->len = 0;
    pkt->type = 0;  /* Invalid type */
    pkt->op = VIRTIO_VSOCK_OP_REQUEST;
    pkt->flags = 0;
    pkt->buf_alloc = 4096;
    pkt->fwd_cnt = 0;

    vv_desc(vr, 0, pkt, sizeof(*pkt), 0, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_Q(V0056, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_type_zero,
                "Packet with type=0 (invalid for NO_IMPLIED_STREAM)",
                VIRTIO_SPEC_V1_3, "5.10.6.6.2", 0);
