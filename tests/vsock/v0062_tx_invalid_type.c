/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0062: Vsock TX with invalid type field.
 *
 * Spec 5.10.6: Submit a vsock packet with type set to 0xFFFF
 * (undefined transport operation). The device must reject the
 * packet rather than misinterpreting the control field.
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

static test_result_t test_vsock_tx_invalid_type(struct virtio_dev *dev,
                                                struct vring *vr)
{
    struct virtio_vsock_hdr *pkt = vv_alloc_pages(1);
    memset(pkt, 0, sizeof(*pkt));

    /* Read guest CID from config */
    volatile uint64_t *cfg = (volatile uint64_t *)dev->device_cfg;
    uint64_t guest_cid = *cfg;

    pkt->src_cid = guest_cid;
    pkt->dst_cid = 2; /* host */
    pkt->src_port = 1234;
    pkt->dst_port = 5678;
    pkt->len = 0;
    pkt->type = 0xFFFF; /* invalid */
    pkt->op = 1; /* VIRTIO_VSOCK_OP_REQUEST */

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(pkt), sizeof(*pkt),
                       0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(V0062, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_tx_invalid_type,
                "Vsock TX with invalid type field",
                VIRTIO_SPEC_V1_2, "5.10.6", 1);
