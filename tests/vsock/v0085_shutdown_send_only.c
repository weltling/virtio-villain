/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0085: shutdown send direction only.
 *
 * Spec 5.10.6.6: SHUTDOWN with only VIRTIO_VSOCK_SHUTDOWN_SEND set
 * closes the send direction while leaving receive open.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_vsock_shutdown_send(struct virtio_dev *dev,
                                             struct vring *vr)
{
    struct virtio_vsock_hdr *conn = vv_alloc_pages(1);
    conn->src_cid = 3; conn->dst_cid = 2;
    conn->src_port = 1200; conn->dst_port = 1200;
    conn->len = 0; conn->type = VIRTIO_VSOCK_TYPE_STREAM;
    conn->op = VIRTIO_VSOCK_OP_REQUEST;
    conn->flags = 0; conn->buf_alloc = 65536; conn->fwd_cnt = 0;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(conn), sizeof(*conn), 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;

    struct virtio_vsock_hdr *shut = vv_alloc_pages(1);
    shut->src_cid = 3; shut->dst_cid = 2;
    shut->src_port = 1200; shut->dst_port = 1200;
    shut->len = 0; shut->type = VIRTIO_VSOCK_TYPE_STREAM;
    shut->op = VIRTIO_VSOCK_OP_SHUTDOWN;
    shut->flags = VIRTIO_VSOCK_SHUTDOWN_SEND;
    shut->buf_alloc = 65536; shut->fwd_cnt = 0;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(shut), sizeof(*shut), 0, 0);
    vring_raw_set_avail(vr, 1, 0);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(V0085, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_shutdown_send,
              "Shutdown send direction only",
              VIRTIO_SPEC_V1_2, "5.10.6.6");
