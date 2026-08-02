/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0092: vsock RW payload descriptor whose addr plus len wraps 2^64.
 *
 * Spec 2.7.5: addr and len define a guest physical buffer. A device that
 * builds a combined range for the tx payload by adding addr and len can
 * walk a wrapped range and reach unrelated memory. Establish a stream
 * connection, then send an OP_RW whose payload descriptor sits near the
 * top of the address space with a length that makes addr plus len wrap
 * to a low value. The device must reject the range or stay alive.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_vsock_rw_addr_len_wrap(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    struct virtio_vsock_hdr *conn = vv_alloc_pages(1);
    conn->src_cid = 3; conn->dst_cid = 2;
    conn->src_port = 4200; conn->dst_port = 4200;
    conn->len = 0;
    conn->type = VIRTIO_VSOCK_TYPE_STREAM;
    conn->op = VIRTIO_VSOCK_OP_REQUEST;
    conn->flags = 0;
    conn->buf_alloc = 65536; conn->fwd_cnt = 0;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(conn), sizeof(*conn), 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;

    struct virtio_vsock_hdr *rw = vv_alloc_pages(1);
    rw->src_cid = 3; rw->dst_cid = 2;
    rw->src_port = 4200; rw->dst_port = 4200;
    rw->len = 0x2000;
    rw->type = VIRTIO_VSOCK_TYPE_STREAM;
    rw->op = VIRTIO_VSOCK_OP_RW;
    rw->flags = 0;
    rw->buf_alloc = 65536; rw->fwd_cnt = 0;

    /*
     * Payload descriptor addr + len wraps past 2^64. The header is a
     * normal readable buffer; only the data region is malformed.
     * desc[1].addr = 0xFFFFFFFF_FFFFF000, len = 0x2000 -> end 0x1000.
     */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(rw), sizeof(*rw),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, 0xFFFFFFFFFFFFF000ULL, 0x2000, 0, 0);
    vring_raw_set_avail(vr, 1, 0);
    vring_raw_set_avail_idx(vr, 2);

    r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r == TEST_FAIL)
        TFAIL("device reported failure on a wrapping payload region");
    if (r == TEST_PASS || r == TEST_WEDGED)
        return r;
    TREJECT("no device response within timeout");
}

REGISTER_TEST(V0092, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_rw_addr_len_wrap,
              "RW payload descriptor addr plus len wraps 64 bits",
              VIRTIO_SPEC_V1_2, "2.7.5");
