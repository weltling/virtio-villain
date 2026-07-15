/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0080: vsock stream RW data send on established connection.
 *
 * Spec 5.10.6.3: After CONNECT the driver may send data via OP_RW.
 * Send a CONNECT request followed by an RW with 32 bytes of payload.
 * The device must consume both packets.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_vsock_stream_rw(struct virtio_dev *dev,
                                          struct vring *vr)
{
    /* CONNECT */
    struct virtio_vsock_hdr *conn = vv_alloc_pages(1);
    conn->src_cid = 3; conn->dst_cid = 2;
    conn->src_port = 4000; conn->dst_port = 4000;
    conn->len = 0;
    conn->type = VIRTIO_VSOCK_TYPE_STREAM;
    conn->op = VIRTIO_VSOCK_OP_REQUEST;
    conn->flags = 0;
    conn->buf_alloc = 65536; conn->fwd_cnt = 0;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(conn), sizeof(*conn), 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;

    /* RW with payload */
    struct virtio_vsock_hdr *rw = vv_alloc_pages(1);
    uint8_t *payload = vv_alloc_pages(1);

    rw->src_cid = 3; rw->dst_cid = 2;
    rw->src_port = 4000; rw->dst_port = 4000;
    rw->len = 32;
    rw->type = VIRTIO_VSOCK_TYPE_STREAM;
    rw->op = VIRTIO_VSOCK_OP_RW;
    rw->flags = 0;
    rw->buf_alloc = 65536; rw->fwd_cnt = 0;

    memset(payload, 0x61, 32);  /* 'a' repeated */

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(rw), sizeof(*rw),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(payload), 32, 0, 0);
    vring_raw_set_avail(vr, 1, 0);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(V0080, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_stream_rw,
              "Stream RW data send on established connection",
              VIRTIO_SPEC_V1_2, "5.10.6.3");
