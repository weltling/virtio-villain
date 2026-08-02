/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0094: vsock RW payload where a contiguous region base equals a wrapped
 * boundary.
 *
 * Spec 2.7.5: addr and len define a guest physical buffer. A device that
 * folds the tx payload into one range by adding addr and len per region
 * leaves the wrapped end of a first region as the running boundary. A
 * second region whose base equals that wrapped end is treated as
 * contiguous with an empty accumulated page list, which a device may
 * index without a guard. The vsock tx path builds the range with a strict
 * expectation, so an inconsistent range is a release build failure, not
 * only a debug one.
 *
 * Establish a stream connection, then send an OP_RW whose payload is two
 * descriptors: the first wraps so its end lands at 0x100, and the second
 * starts at 0x100 so it is GPA contiguous. The device must reject the
 * range or stay alive.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_vsock_rw_contig_wrap(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_vsock_hdr *conn = vv_alloc_pages(1);
    conn->src_cid = 3; conn->dst_cid = 2;
    conn->src_port = 4400; conn->dst_port = 4400;
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
    rw->src_port = 4400; rw->dst_port = 4400;
    rw->len = 1024;
    rw->type = VIRTIO_VSOCK_TYPE_STREAM;
    rw->op = VIRTIO_VSOCK_OP_RW;
    rw->flags = 0;
    rw->buf_alloc = 65536; rw->fwd_cnt = 0;

    /*
     * Payload split across two readable descriptors. The first wraps so
     * its end lands at 0x100, and the second starts at 0x100 so it is
     * contiguous with the wrapped end.
     */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(rw), sizeof(*rw),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, 0xFFFFFFFFFFFFFF00ULL, 512,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, 0x100, 512, 0, 0);
    vring_raw_set_avail(vr, 1, 0);
    vring_raw_set_avail_idx(vr, 2);

    r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r == TEST_FAIL)
        TFAIL("device reported failure on a contiguous wrapped region");
    if (r == TEST_PASS || r == TEST_WEDGED)
        return r;
    TREJECT("no device response within timeout");
}

REGISTER_TEST(V0094, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_rw_contig_wrap,
              "RW payload contiguous region base equals a wrapped end",
              VIRTIO_SPEC_V1_2, "2.7.5");
