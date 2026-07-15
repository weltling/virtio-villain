/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0083: CREDIT_REQUEST on established connection.
 *
 * Spec 5.10.6.3: The driver may send OP_CREDIT_REQUEST to ask the
 * peer for a credit update. Send CONNECT then CREDIT_REQUEST. The
 * device must consume both packets.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_vsock_credit_request(struct virtio_dev *dev,
                                               struct vring *vr)
{
    /* CONNECT */
    struct virtio_vsock_hdr *conn = vv_alloc_pages(1);
    conn->src_cid = 3; conn->dst_cid = 2;
    conn->src_port = 5500; conn->dst_port = 5500;
    conn->len = 0; conn->type = VIRTIO_VSOCK_TYPE_STREAM;
    conn->op = VIRTIO_VSOCK_OP_REQUEST;
    conn->flags = 0; conn->buf_alloc = 65536; conn->fwd_cnt = 0;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(conn), sizeof(*conn), 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;

    /* CREDIT_REQUEST */
    struct virtio_vsock_hdr *creq = vv_alloc_pages(1);
    creq->src_cid = 3; creq->dst_cid = 2;
    creq->src_port = 5500; creq->dst_port = 5500;
    creq->len = 0; creq->type = VIRTIO_VSOCK_TYPE_STREAM;
    creq->op = VIRTIO_VSOCK_OP_CREDIT_REQUEST;
    creq->flags = 0; creq->buf_alloc = 65536; creq->fwd_cnt = 0;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(creq), sizeof(*creq), 0, 0);
    vring_raw_set_avail(vr, 1, 0);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(V0083, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_credit_request,
              "CREDIT_REQUEST on established connection",
              VIRTIO_SPEC_V1_2, "5.10.6.3");
