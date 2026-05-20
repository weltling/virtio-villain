/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0063: Vsock TX with mismatched len and data.
 *
 * Spec 5.10.6: Submit a vsock packet header claiming len=4096 but
 * provide only a 64 byte data descriptor. The device must not read
 * beyond the actual descriptor length.
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

static test_result_t test_vsock_tx_len_mismatch(struct virtio_dev *dev,
                                                struct vring *vr)
{
    uint8_t *page = vv_alloc_pages(1);
    memset(page, 0, 4096);

    struct virtio_vsock_hdr *pkt = (struct virtio_vsock_hdr *)page;
    uint8_t *data = page + sizeof(*pkt);

    volatile uint64_t *cfg = (volatile uint64_t *)dev->device_cfg;
    uint64_t guest_cid = *cfg;

    pkt->src_cid = guest_cid;
    pkt->dst_cid = 2;
    pkt->src_port = 1234;
    pkt->dst_port = 5678;
    pkt->len = 4096; /* claims 4096 bytes of payload */
    pkt->type = VIRTIO_VSOCK_TYPE_STREAM;
    pkt->op = 1; /* REQUEST */

    memset(data, 0xAA, 64);

    /* Header descriptor */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(pkt), sizeof(*pkt),
                       VRING_DESC_F_NEXT, 1);
    /* Data: only 64 bytes, not 4096 as claimed */
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 64, 0, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(V0063, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_tx_len_mismatch,
                "Vsock TX with header len exceeding data descriptor",
                VIRTIO_SPEC_V1_2, "5.10.6", 1);
