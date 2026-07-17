/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0090: data on two independent connections.
 *
 * Open two connections on different ports, then send an RW packet
 * on each. The device must route data to the correct connection
 * context. Tests independent per-connection data handling.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static void mkconn(struct virtio_vsock_hdr *h, uint32_t port, uint16_t op)
{
    h->src_cid = 3; h->dst_cid = 2;
    h->src_port = port; h->dst_port = port;
    h->len = 0; h->type = VIRTIO_VSOCK_TYPE_STREAM;
    h->op = op; h->flags = 0;
    h->buf_alloc = 65536; h->fwd_cnt = 0;
}

static test_result_t test_vsock_two_conn_data(struct virtio_dev *dev,
                                             struct vring *vr)
{
    /* CONNECT port 5100 */
    struct virtio_vsock_hdr *c1 = vv_alloc_pages(1);
    mkconn(c1, 5100, VIRTIO_VSOCK_OP_REQUEST);
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(c1), sizeof(*c1), 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;

    /* CONNECT port 5101 */
    struct virtio_vsock_hdr *c2 = vv_alloc_pages(1);
    mkconn(c2, 5101, VIRTIO_VSOCK_OP_REQUEST);
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(c2), sizeof(*c2), 0, 0);
    vring_raw_set_avail(vr, 1, 0);
    vring_raw_set_avail_idx(vr, 2);
    r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;

    /* RW on both connections in one batch */
    struct virtio_vsock_hdr *d1 = vv_alloc_pages(1);
    uint8_t *p1 = vv_alloc_pages(1);
    struct virtio_vsock_hdr *d2 = vv_alloc_pages(1);
    uint8_t *p2 = vv_alloc_pages(1);

    mkconn(d1, 5100, VIRTIO_VSOCK_OP_RW); d1->len = 16;
    mkconn(d2, 5101, VIRTIO_VSOCK_OP_RW); d2->len = 16;
    memset(p1, 0xA1, 16);
    memset(p2, 0xB2, 16);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(d1), sizeof(*d1),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(p1), 16, 0, 0);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(d2), sizeof(*d2),
                       VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, vv_virt_to_phys(p2), 16, 0, 0);
    vring_raw_set_avail(vr, 2, 0);
    vring_raw_set_avail(vr, 3, 2);
    vring_raw_set_avail_idx(vr, 4);

    return vv_kick_and_wait_n(dev, vr, 0, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST(V0090, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_two_conn_data,
              "Data on two independent connections",
              VIRTIO_SPEC_V1_2, "5.10.6");
