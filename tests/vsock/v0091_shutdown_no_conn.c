/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0091: SHUTDOWN for a nonexistent connection.
 *
 * Spec 5.10.6.6: Send a SHUTDOWN on a port pair that was never
 * connected. The device should respond with RST or silently drop
 * it, but must not crash or corrupt state.
 */
#include "lib/virtio_test.h"

static test_result_t test_vsock_shutdown_no_conn(struct virtio_dev *dev,
                                                struct vring *vr)
{
    struct virtio_vsock_hdr *shut = vv_alloc_pages(1);

    shut->src_cid = 3; shut->dst_cid = 2;
    shut->src_port = 6600; shut->dst_port = 6600;
    shut->len = 0; shut->type = VIRTIO_VSOCK_TYPE_STREAM;
    shut->op = VIRTIO_VSOCK_OP_SHUTDOWN;
    shut->flags = VIRTIO_VSOCK_SHUTDOWN_BOTH;
    shut->buf_alloc = 0; shut->fwd_cnt = 0;

    vv_desc(vr, 0, shut, sizeof(*shut), 0, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(V0091, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_shutdown_no_conn,
              "SHUTDOWN for a nonexistent connection",
              VIRTIO_SPEC_V1_2, "5.10.6.6");
