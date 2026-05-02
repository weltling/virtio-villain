/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0017: vsock_multiple_connect_same_port
 *
 * Send multiple CONNECT requests to the same dst_port from different
 * src_ports simultaneously. Tests whether the device handles concurrent
 * connection attempts without confusion or state corruption.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

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

#define VIRTIO_VSOCK_TYPE_STREAM 1
#define VIRTIO_VSOCK_OP_REQUEST  1

static test_result_t test_vsock_multi_connect(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_vsock_hdr *pkt0 = vv_alloc_pages(1);
    struct virtio_vsock_hdr *pkt1 = vv_alloc_pages(1);
    struct virtio_vsock_hdr *pkt2 = vv_alloc_pages(1);

    /* Three CONNECTs to same dst_port from different src_ports */
    for (int i = 0; i < 3; i++) {
        struct virtio_vsock_hdr *p = (i == 0) ? pkt0 :
                                     (i == 1) ? pkt1 : pkt2;
        p->src_cid = 3;
        p->dst_cid = 2;
        p->src_port = 1000 + i;
        p->dst_port = 5678; /* same destination */
        p->len = 0;
        p->type = VIRTIO_VSOCK_TYPE_STREAM;
        p->op = VIRTIO_VSOCK_OP_REQUEST;
        p->flags = 0;
        p->buf_alloc = 4096;
        p->fwd_cnt = 0;
    }

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(pkt0), sizeof(*pkt0), 0, 0);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(pkt1), sizeof(*pkt1), 0, 0);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(pkt2), sizeof(*pkt2), 0, 0);

    /* Submit all three */
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 1);
    vring_raw_set_avail(vr, 2, 2);
    vring_raw_set_avail_idx(vr, 3);

    return vv_kick_and_wait_n(dev, vr, 0, 3, VV_TIMEOUT_MS);
}

REGISTER_TEST(V0017, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_multi_connect,
              "Multiple CONNECTs to same port from different sources",
              VIRTIO_SPEC_V1_2, "5.10.6");
