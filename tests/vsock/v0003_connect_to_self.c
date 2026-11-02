/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0003: vsock_connect_to_self
 *
 * Submit a CONNECT where src_cid == dst_cid (connecting to self).
 * The device must reject this as an invalid connection.
 */
#include "lib/virtio_test.h"

static test_result_t test_vsock_connect_self(struct virtio_dev *dev,
                                             struct vring *vr)
{
    struct virtio_vsock_hdr *pkt = vv_alloc_pages(1);

    /* Both src and dst are the same CID */
    pkt->src_cid = 3;
    pkt->dst_cid = 3;
    pkt->src_port = 1234;
    pkt->dst_port = 5678;
    pkt->len = 0;
    pkt->type = VIRTIO_VSOCK_TYPE_STREAM;
    pkt->op = VIRTIO_VSOCK_OP_REQUEST;
    pkt->flags = 0;
    pkt->buf_alloc = 4096;
    pkt->fwd_cnt = 0;

    vv_desc(vr, 0, pkt, sizeof(*pkt), 0, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(V0003, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_connect_self,
              "CONNECT with src_cid == dst_cid",
              VIRTIO_SPEC_V1_2, "5.10.6");
