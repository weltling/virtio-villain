/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0087: connect then immediate RST (abort connection).
 *
 * Spec 5.10.6: Send CONNECT immediately followed by RST on the
 * same port, aborting the connection before it is established.
 * The device must handle the abort without crashing.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_vsock_connect_rst(struct virtio_dev *dev,
                                            struct vring *vr)
{
    struct virtio_vsock_hdr *conn = vv_alloc_pages(1);
    struct virtio_vsock_hdr *rst = vv_alloc_pages(1);

    conn->src_cid = 3; conn->dst_cid = 2;
    conn->src_port = 8800; conn->dst_port = 8800;
    conn->len = 0; conn->type = VIRTIO_VSOCK_TYPE_STREAM;
    conn->op = VIRTIO_VSOCK_OP_REQUEST;
    conn->flags = 0; conn->buf_alloc = 65536; conn->fwd_cnt = 0;

    rst->src_cid = 3; rst->dst_cid = 2;
    rst->src_port = 8800; rst->dst_port = 8800;
    rst->len = 0; rst->type = VIRTIO_VSOCK_TYPE_STREAM;
    rst->op = VIRTIO_VSOCK_OP_RST;
    rst->flags = 0; rst->buf_alloc = 0; rst->fwd_cnt = 0;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(conn), sizeof(*conn), 0, 0);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(rst), sizeof(*rst), 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 1);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait_n(dev, vr, 0, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST(V0087, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_connect_rst,
              "Connect then immediate RST aborts cleanly",
              VIRTIO_SPEC_V1_2, "5.10.6");
