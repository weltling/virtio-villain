/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0067: vsock OP_RW with src_cid == dst_cid.
 *
 * Spec 5.10.6.6: A vsock data packet identifies the sender and
 * the receiver by their CIDs. Send OP_RW with src_cid set
 * equal to dst_cid. The device must drop the malformed packet
 * rather than looping it back as if the host were talking to
 * itself and corrupting per connection bookkeeping.
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
#define VIRTIO_VSOCK_OP_RW       5

static test_result_t test_vsock_rw_self_cid(struct virtio_dev *dev,
                                            struct vring *vr)
{
    struct virtio_vsock_hdr *hdr = vv_alloc_pages(1);
    uint8_t *payload = vv_alloc_pages(1);

    memset(hdr, 0, sizeof(*hdr));
    hdr->src_cid   = 3;
    hdr->dst_cid   = 3;
    hdr->src_port  = 0x1111;
    hdr->dst_port  = 0x2222;
    hdr->type      = VIRTIO_VSOCK_TYPE_STREAM;
    hdr->op        = VIRTIO_VSOCK_OP_RW;
    hdr->len       = 64;
    hdr->buf_alloc = 0x10000;

    memset(payload, 'Z', 64);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(payload), 64, 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(V0067, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_rw_self_cid,
              "OP_RW with src_cid equal to dst_cid",
              VIRTIO_SPEC_V1_2, "5.10.6.6");
