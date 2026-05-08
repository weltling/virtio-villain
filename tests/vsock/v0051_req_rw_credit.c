/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0051: REQUEST then RW then CREDIT_REQUEST drained from TX
 *
 * Spec 5.10.6 says the device must accept and drain stream
 * packets in submission order regardless of connection state. No
 * real listener exists for the chosen port pair so the device
 * will likely respond with RST on the RX queue, but the TX side
 * must still drain a REQUEST followed by an RW data carrying
 * packet followed by a CREDIT_REQUEST without wedging. Catches
 * devices that stop draining once they decide a flow is dead.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>

struct virtio_vsock_hdr {
    uint64_t src_cid;
    uint64_t dst_cid;
    uint32_t src_port;
    uint32_t dst_port;
    uint32_t len;
    uint16_t type;
    uint16_t op;
    uint32_t flags;
    uint32_t buf_alloc;
    uint32_t fwd_cnt;
} __attribute__((packed));

#define VIRTIO_VSOCK_TYPE_STREAM       1
#define VIRTIO_VSOCK_OP_REQUEST        1
#define VIRTIO_VSOCK_OP_RW             5
#define VIRTIO_VSOCK_OP_CREDIT_REQUEST 7

static void fill(struct virtio_vsock_hdr *h, uint16_t op, uint32_t len)
{
    h->src_cid = 3;
    h->dst_cid = 2;
    h->src_port = 31337;
    h->dst_port = 31338;
    h->len = len;
    h->type = VIRTIO_VSOCK_TYPE_STREAM;
    h->op = op;
    h->flags = 0;
    h->buf_alloc = 65536;
    h->fwd_cnt = 0;
}

static test_result_t test_vsock_req_rw_credit(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_vsock_hdr *p0 = vv_alloc_pages(1);
    struct virtio_vsock_hdr *p1 = vv_alloc_pages(1);
    uint8_t *payload = (uint8_t *)p1 + sizeof(*p1);
    struct virtio_vsock_hdr *p2 = vv_alloc_pages(1);

    fill(p0, VIRTIO_VSOCK_OP_REQUEST, 0);
    fill(p1, VIRTIO_VSOCK_OP_RW, 16);
    memset(payload, 'A', 16);
    fill(p2, VIRTIO_VSOCK_OP_CREDIT_REQUEST, 0);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(p0), sizeof(*p0), 0, 0);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(p1), sizeof(*p1) + 16, 0, 0);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(p2), sizeof(*p2), 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 1);
    vring_raw_set_avail(vr, 2, 2);
    vring_raw_set_avail_idx(vr, 3);

    return vv_kick_and_wait_n(dev, vr, 0, 3, VV_TIMEOUT_MS);
}

REGISTER_TEST(V0051, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_req_rw_credit,
              "REQUEST then RW then CREDIT_REQUEST drained from TX",
              VIRTIO_SPEC_V1_2, "5.10.6");
