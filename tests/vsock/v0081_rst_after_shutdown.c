/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0081: RST after SHUTDOWN completes the close sequence.
 *
 * Spec 5.10.6.6: After sending SHUTDOWN, the peer should respond
 * with RST to confirm the connection is fully closed. Send CONNECT,
 * then SHUTDOWN(BOTH), then RST. The device must consume all three
 * packets representing a complete graceful close sequence.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_vsock_rst_after_shutdown(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    struct virtio_vsock_hdr *conn = vv_alloc_pages(1);
    struct virtio_vsock_hdr *shut = vv_alloc_pages(1);
    struct virtio_vsock_hdr *rst = vv_alloc_pages(1);

    /* CONNECT */
    conn->src_cid = 3; conn->dst_cid = 2;
    conn->src_port = 3000; conn->dst_port = 3000;
    conn->len = 0; conn->type = VIRTIO_VSOCK_TYPE_STREAM;
    conn->op = VIRTIO_VSOCK_OP_REQUEST;
    conn->flags = 0; conn->buf_alloc = 65536; conn->fwd_cnt = 0;

    /* SHUTDOWN both directions */
    shut->src_cid = 3; shut->dst_cid = 2;
    shut->src_port = 3000; shut->dst_port = 3000;
    shut->len = 0; shut->type = VIRTIO_VSOCK_TYPE_STREAM;
    shut->op = VIRTIO_VSOCK_OP_SHUTDOWN;
    shut->flags = VIRTIO_VSOCK_SHUTDOWN_BOTH;
    shut->buf_alloc = 65536; shut->fwd_cnt = 0;

    /* RST to confirm close */
    rst->src_cid = 3; rst->dst_cid = 2;
    rst->src_port = 3000; rst->dst_port = 3000;
    rst->len = 0; rst->type = VIRTIO_VSOCK_TYPE_STREAM;
    rst->op = VIRTIO_VSOCK_OP_RST;
    rst->flags = 0; rst->buf_alloc = 0; rst->fwd_cnt = 0;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(conn), sizeof(*conn), 0, 0);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(shut), sizeof(*shut), 0, 0);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(rst), sizeof(*rst), 0, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 1);
    vring_raw_set_avail(vr, 2, 2);
    vring_raw_set_avail_idx(vr, 3);

    return vv_kick_and_wait_n(dev, vr, 0, 3, VV_TIMEOUT_MS);
}

REGISTER_TEST(V0081, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_rst_after_shutdown,
              "RST after SHUTDOWN completes graceful close",
              VIRTIO_SPEC_V1_2, "5.10.6.6");
