/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0089: two consecutive RW packets on same connection.
 *
 * Send CONNECT then two RW packets with different payloads on the
 * same port. The device must consume all three in sequence,
 * testing the device maintains connection state across multiple
 * data packets.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_vsock_two_rw(struct virtio_dev *dev, struct vring *vr)
{
    struct virtio_vsock_hdr *conn = vv_alloc_pages(1);
    conn->src_cid = 3; conn->dst_cid = 2;
    conn->src_port = 6600; conn->dst_port = 6600;
    conn->len = 0; conn->type = VIRTIO_VSOCK_TYPE_STREAM;
    conn->op = VIRTIO_VSOCK_OP_REQUEST;
    conn->flags = 0; conn->buf_alloc = 65536; conn->fwd_cnt = 0;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(conn), sizeof(*conn), 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;

    /* RW packet 1 */
    struct virtio_vsock_hdr *rw1 = vv_alloc_pages(1);
    uint8_t *p1 = vv_alloc_pages(1);
    rw1->src_cid = 3; rw1->dst_cid = 2;
    rw1->src_port = 6600; rw1->dst_port = 6600;
    rw1->len = 16; rw1->type = VIRTIO_VSOCK_TYPE_STREAM;
    rw1->op = VIRTIO_VSOCK_OP_RW;
    rw1->flags = 0; rw1->buf_alloc = 65536; rw1->fwd_cnt = 0;
    memset(p1, 'A', 16);

    /* RW packet 2 */
    struct virtio_vsock_hdr *rw2 = vv_alloc_pages(1);
    uint8_t *p2 = vv_alloc_pages(1);
    rw2->src_cid = 3; rw2->dst_cid = 2;
    rw2->src_port = 6600; rw2->dst_port = 6600;
    rw2->len = 16; rw2->type = VIRTIO_VSOCK_TYPE_STREAM;
    rw2->op = VIRTIO_VSOCK_OP_RW;
    rw2->flags = 0; rw2->buf_alloc = 65536; rw2->fwd_cnt = 16;
    memset(p2, 'B', 16);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(rw1), sizeof(*rw1),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(p1), 16, 0, 0);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(rw2), sizeof(*rw2),
                       VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, vv_virt_to_phys(p2), 16, 0, 0);

    vring_raw_set_avail(vr, 1, 0);
    vring_raw_set_avail(vr, 2, 2);
    vring_raw_set_avail_idx(vr, 3);

    return vv_kick_and_wait_n(dev, vr, 0, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST(V0089, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_two_rw,
              "Two consecutive RW packets on same connection",
              VIRTIO_SPEC_V1_2, "5.10.6.3");
