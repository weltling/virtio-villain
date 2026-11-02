/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0018: vsock_connect_zero_buf_alloc
 *
 * Send a CONNECT (REQUEST) with buf_alloc=0, indicating the guest
 * has no receive buffer space. The device/host should still accept
 * the connection but not send data until credit is updated.
 */
#include "lib/virtio_test.h"

static test_result_t test_vsock_zero_buf_alloc(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_vsock_hdr *pkt = vv_alloc_pages(1);

    pkt->src_cid = 3;
    pkt->dst_cid = 2;
    pkt->src_port = 1234;
    pkt->dst_port = 5678;
    pkt->len = 0;
    pkt->type = VIRTIO_VSOCK_TYPE_STREAM;
    pkt->op = VIRTIO_VSOCK_OP_REQUEST;
    pkt->flags = 0;
    pkt->buf_alloc = 0; /* zero buffer allocation */
    pkt->fwd_cnt = 0;

    vv_desc(vr, 0, pkt, sizeof(*pkt), 0, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(V0018, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_zero_buf_alloc,
              "CONNECT with buf_alloc=0 (no receive space)",
              VIRTIO_SPEC_V1_2, "5.10.6.3");
