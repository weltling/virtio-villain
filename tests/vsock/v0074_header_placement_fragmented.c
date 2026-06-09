/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0074: header fragmented across multiple descriptors.
 *
 * v1.4 5.10.6: the header may straddle descriptor boundaries.
 * Place sizeof(hdr)/2 bytes in the first desc and the rest in
 * the second. The device must reassemble the header. A device
 * that reads only the first descriptor will see truncated
 * fields.
 */
#include "tests/test.h"
#include "lib/util.h"

#include <string.h>

#define VIRTIO_VSOCK_TYPE_STREAM 1
#define VIRTIO_VSOCK_OP_REQUEST  1

struct virtio_vsock_hdr {
    uint64_t src_cid; uint64_t dst_cid;
    uint32_t src_port; uint32_t dst_port;
    uint32_t len; uint16_t type; uint16_t op;
    uint32_t flags; uint32_t buf_alloc; uint32_t fwd_cnt;
} __attribute__((packed));

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    uint8_t *p1 = vv_alloc_pages(1);
    uint8_t *p2 = vv_alloc_pages(1);
    struct virtio_vsock_hdr tmp;
    memset(&tmp, 0, sizeof(tmp));
    tmp.src_cid = 3; tmp.dst_cid = 2;
    tmp.src_port = 1; tmp.dst_port = 2;
    tmp.type = VIRTIO_VSOCK_TYPE_STREAM;
    tmp.op = VIRTIO_VSOCK_OP_REQUEST;

    uint32_t half = sizeof(tmp) / 2;
    memcpy(p1, &tmp, half);
    memcpy(p2, (uint8_t *)&tmp + half, sizeof(tmp) - half);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(p1), half,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(p2),
                       (uint32_t)(sizeof(tmp) - half), 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(V0074, VIRTIO_PCI_DEVICE_VSOCK, test,
              "Header fragmented across two descriptors",
              VIRTIO_SPEC_V1_4, "5.10.6");
