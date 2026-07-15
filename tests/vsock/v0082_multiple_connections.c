/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0082: two independent connections on different ports.
 *
 * Open two CONNECT requests on different port pairs simultaneously.
 * The device must handle multiple independent connection contexts.
 * Spec 5.10.6.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_vsock_multi_conn(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_vsock_hdr *c1 = vv_alloc_pages(1);
    struct virtio_vsock_hdr *c2 = vv_alloc_pages(1);

    c1->src_cid = 3; c1->dst_cid = 2;
    c1->src_port = 2000; c1->dst_port = 2000;
    c1->len = 0; c1->type = VIRTIO_VSOCK_TYPE_STREAM;
    c1->op = VIRTIO_VSOCK_OP_REQUEST;
    c1->flags = 0; c1->buf_alloc = 65536; c1->fwd_cnt = 0;

    c2->src_cid = 3; c2->dst_cid = 2;
    c2->src_port = 2001; c2->dst_port = 2001;
    c2->len = 0; c2->type = VIRTIO_VSOCK_TYPE_STREAM;
    c2->op = VIRTIO_VSOCK_OP_REQUEST;
    c2->flags = 0; c2->buf_alloc = 65536; c2->fwd_cnt = 0;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(c1), sizeof(*c1), 0, 0);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(c2), sizeof(*c2), 0, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 1);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait_n(dev, vr, 0, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST(V0082, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_multi_conn,
              "Two connections on different ports",
              VIRTIO_SPEC_V1_2, "5.10.6");
