/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0022: vsock_credit_request_no_connection
 *
 * Send a CREDIT_REQUEST for a port pair where no connection exists.
 * The device must not crash when asked for credit info on a
 * nonexistent connection.
 */
#include "lib/virtio_test.h"

static test_result_t test_vsock_credit_req_noconn(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    struct virtio_vsock_hdr *pkt = vv_alloc_pages(1);

    pkt->src_cid = 3;
    pkt->dst_cid = 2;
    pkt->src_port = 7777;
    pkt->dst_port = 7777;
    pkt->len = 0;
    pkt->type = VIRTIO_VSOCK_TYPE_STREAM;
    pkt->op = VIRTIO_VSOCK_OP_CREDIT_REQUEST;
    pkt->flags = 0;
    pkt->buf_alloc = 4096;
    pkt->fwd_cnt = 0;

    vv_desc(vr, 0, pkt, sizeof(*pkt), 0, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(V0022, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_credit_req_noconn,
              "CREDIT_REQUEST for nonexistent connection",
              VIRTIO_SPEC_V1_2, "5.10.6.5");
