/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0026: vsock_connect_max_port
 *
 * Send a CONNECT request with src_port set to UINT32_MAX.
 * The device must handle extreme port values without overflow
 * in any internal port-to-connection mapping structures.
 */
#include "lib/virtio_test.h"

static test_result_t test_vsock_connect_max_port(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    struct virtio_vsock_hdr *pkt = vv_alloc_pages(1);

    pkt->src_cid = 3;  /* our guest CID */
    pkt->dst_cid = 2;  /* host */
    pkt->src_port = 0xFFFFFFFF; /* UINT32_MAX */
    pkt->dst_port = 1234;
    pkt->len = 0;
    pkt->type = VIRTIO_VSOCK_TYPE_STREAM;
    pkt->op = VIRTIO_VSOCK_OP_REQUEST;
    pkt->flags = 0;
    pkt->buf_alloc = 4096;
    pkt->fwd_cnt = 0;

    vv_desc(vr, 0, pkt, sizeof(*pkt), 0, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(V0026, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_connect_max_port,
              "CONNECT with src_port = UINT32_MAX",
              VIRTIO_SPEC_V1_2, "5.10.6");
