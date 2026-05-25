/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0068: vsock OP_RW with fwd_cnt far ahead of any sent data.
 *
 * Spec 5.10.6.3: fwd_cnt reports the number of bytes the peer
 * has processed. A driver may not legitimately acknowledge
 * bytes the device never sent. Submit OP_RW with fwd_cnt =
 * 0x7FFFFFFF on a fresh queue. A device that subtracts
 * fwd_cnt from its sent counter to compute outstanding bytes
 * can underflow to a huge unsigned value and stall the
 * connection or wedge accounting. The device must clamp or
 * reject the bogus acknowledgement.
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

#define VIRTIO_VSOCK_TYPE_STREAM 1
#define VIRTIO_VSOCK_OP_RW       5

static test_result_t test_vsock_fwd_cnt_huge(struct virtio_dev *dev,
                                             struct vring *vr)
{
    struct virtio_vsock_hdr *hdr = vv_alloc_pages(1);
    uint8_t *payload = vv_alloc_pages(1);

    memset(hdr, 0, sizeof(*hdr));
    hdr->src_cid   = 3;
    hdr->dst_cid   = 2;
    hdr->src_port  = 0x3333;
    hdr->dst_port  = 0x4444;
    hdr->type      = VIRTIO_VSOCK_TYPE_STREAM;
    hdr->op        = VIRTIO_VSOCK_OP_RW;
    hdr->len       = 16;
    hdr->buf_alloc = 0x10000;
    hdr->fwd_cnt   = 0x7FFFFFFFu;

    memset(payload, 'Q', 16);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(payload), 16, 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(V0068, VIRTIO_PCI_DEVICE_VSOCK,
              test_vsock_fwd_cnt_huge,
              "OP_RW with fwd_cnt = 0x7FFFFFFF",
              VIRTIO_SPEC_V1_2, "5.10.6.3");
