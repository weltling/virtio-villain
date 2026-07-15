/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0086: full connection lifecycle: connect, data, shutdown, rst.
 *
 * Exercise the complete vsock stream lifecycle in one test:
 * CONNECT, RW data, SHUTDOWN(BOTH), RST. All four must be consumed.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_vsock_lifecycle(struct virtio_dev *dev,
                                         struct vring *vr)
{
    struct virtio_vsock_hdr *conn = vv_alloc_pages(1);
    struct virtio_vsock_hdr *rw = vv_alloc_pages(1);
    uint8_t *payload = vv_alloc_pages(1);
    struct virtio_vsock_hdr *shut = vv_alloc_pages(1);
    struct virtio_vsock_hdr *rst = vv_alloc_pages(1);

    /* CONNECT */
    conn->src_cid = 3; conn->dst_cid = 2;
    conn->src_port = 9500; conn->dst_port = 9500;
    conn->len = 0; conn->type = VIRTIO_VSOCK_TYPE_STREAM;
    conn->op = VIRTIO_VSOCK_OP_REQUEST;
    conn->flags = 0; conn->buf_alloc = 65536; conn->fwd_cnt = 0;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(conn), sizeof(*conn), 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;

    /* RW */
    rw->src_cid = 3; rw->dst_cid = 2;
    rw->src_port = 9500; rw->dst_port = 9500;
    rw->len = 8; rw->type = VIRTIO_VSOCK_TYPE_STREAM;
    rw->op = VIRTIO_VSOCK_OP_RW;
    rw->flags = 0; rw->buf_alloc = 65536; rw->fwd_cnt = 0;
    memcpy(payload, "testdata", 8);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(rw), sizeof(*rw),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(payload), 8, 0, 0);
    vring_raw_set_avail(vr, 1, 0);
    vring_raw_set_avail_idx(vr, 2);
    r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;

    /* SHUTDOWN */
    shut->src_cid = 3; shut->dst_cid = 2;
    shut->src_port = 9500; shut->dst_port = 9500;
    shut->len = 0; shut->type = VIRTIO_VSOCK_TYPE_STREAM;
    shut->op = VIRTIO_VSOCK_OP_SHUTDOWN;
    shut->flags = VIRTIO_VSOCK_SHUTDOWN_BOTH;
    shut->buf_alloc = 65536; shut->fwd_cnt = 8;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(shut), sizeof(*shut), 0, 0);
    vring_raw_set_avail(vr, 2, 0);
    vring_raw_set_avail_idx(vr, 3);
    r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;

    /* RST */
    rst->src_cid = 3; rst->dst_cid = 2;
    rst->src_port = 9500; rst->dst_port = 9500;
    rst->len = 0; rst->type = VIRTIO_VSOCK_TYPE_STREAM;
    rst->op = VIRTIO_VSOCK_OP_RST;
    rst->flags = 0; rst->buf_alloc = 0; rst->fwd_cnt = 0;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(rst), sizeof(*rst), 0, 0);
    vring_raw_set_avail(vr, 3, 0);
    vring_raw_set_avail_idx(vr, 4);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(V0086, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_lifecycle,
              "Full lifecycle: connect, data, shutdown, rst",
              VIRTIO_SPEC_V1_2, "5.10.6");
