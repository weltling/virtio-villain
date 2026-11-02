/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0057: vsock_no_implied_stream_unknown_type
 *
 * Send a connection request with a completely unknown type value
 * (e.g. 0xFFFF). With or without NO_IMPLIED_STREAM, the device
 * must not crash on unrecognized type values.
 */
#include "lib/virtio_test.h"

static test_result_t test_vsock_unknown_type(struct virtio_dev *dev,
                                             struct vring *vr)
{
    struct virtio_vsock_hdr *pkt = vv_alloc_pages(1);

    pkt->src_cid = 3;
    pkt->dst_cid = 2;
    pkt->src_port = 1234;
    pkt->dst_port = 5678;
    pkt->len = 0;
    pkt->type = 0xFFFF;  /* Unknown type */
    pkt->op = VIRTIO_VSOCK_OP_REQUEST;
    pkt->flags = 0;
    pkt->buf_alloc = 4096;
    pkt->fwd_cnt = 0;

    vv_desc(vr, 0, pkt, sizeof(*pkt), 0, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_Q(V0057, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_unknown_type,
                "Connect request with unknown type value",
                VIRTIO_SPEC_V1_3, "5.10.6.6.2", 0);
