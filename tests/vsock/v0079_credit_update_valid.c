/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0079: vsock credit update with valid parameters.
 *
 * Spec 5.10.6.3: The driver sends CREDIT_UPDATE to inform the device
 * of updated buffer space. Send a CREDIT_UPDATE with a reasonable
 * buf_alloc and fwd_cnt on an established connection. The device must
 * consume the packet without error.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_vsock_credit_update(struct virtio_dev *dev,
                                              struct vring *vr)
{
    /* CONNECT first */
    struct virtio_vsock_hdr *conn = vv_alloc_pages(1);
    conn->src_cid = 3; conn->dst_cid = 2;
    conn->src_port = 6000; conn->dst_port = 6000;
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

    /* CREDIT_UPDATE */
    struct virtio_vsock_hdr *cu = vv_alloc_pages(1);
    cu->src_cid = 3; cu->dst_cid = 2;
    cu->src_port = 6000; cu->dst_port = 6000;
    cu->len = 0;
    cu->type = VIRTIO_VSOCK_TYPE_STREAM;
    cu->op = VIRTIO_VSOCK_OP_CREDIT_UPDATE;
    cu->flags = 0;
    cu->buf_alloc = 131072;  /* increased buffer */
    cu->fwd_cnt = 0;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(cu), sizeof(*cu), 0, 0);
    vring_raw_set_avail(vr, 1, 0);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(V0079, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_credit_update,
              "Credit update with valid increased buf_alloc",
              VIRTIO_SPEC_V1_2, "5.10.6.3");
