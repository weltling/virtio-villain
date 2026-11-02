/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0016: vsock_connect_dst_port_zero
 *
 * Send a CONNECT with dst_port=0. Port 0 may be treated as invalid
 * or reserved by some implementations. The device must not crash.
 */
#include "lib/virtio_test.h"

static test_result_t test_vsock_connect_port_zero(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    struct virtio_vsock_hdr *pkt = vv_alloc_pages(1);

    pkt->src_cid = 3;
    pkt->dst_cid = 2;
    pkt->src_port = 1234;
    pkt->dst_port = 0; /* port zero */
    pkt->len = 0;
    pkt->type = VIRTIO_VSOCK_TYPE_STREAM;
    pkt->op = VIRTIO_VSOCK_OP_REQUEST;
    pkt->flags = 0;
    pkt->buf_alloc = 4096;
    pkt->fwd_cnt = 0;

    vv_desc(vr, 0, pkt, sizeof(*pkt), 0, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(V0016, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_connect_port_zero,
              "CONNECT with dst_port=0",
              VIRTIO_SPEC_V1_2, "5.10.6");
