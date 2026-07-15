/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0076: vsock dgram RW data send.
 *
 * When VIRTIO_VSOCK_F_DGRAM is negotiated, the driver may send
 * connectionless datagrams using OP_RW with type=DGRAM. Unlike
 * stream sockets, no CONNECT handshake is needed. The device must
 * consume the packet without crashing.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_vsock_dgram_rw(struct virtio_dev *dev,
                                         struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1u << VIRTIO_VSOCK_F_DGRAM)))
        return TEST_SKIP;

    struct virtio_vsock_hdr *pkt = vv_alloc_pages(1);
    uint8_t *payload = vv_alloc_pages(1);

    pkt->src_cid = 3;
    pkt->dst_cid = 2;
    pkt->src_port = 9000;
    pkt->dst_port = 9000;
    pkt->len = 32;
    pkt->type = VIRTIO_VSOCK_TYPE_DGRAM;
    pkt->op = VIRTIO_VSOCK_OP_RW;
    pkt->flags = 0;
    pkt->buf_alloc = 0;  /* no credit for dgram */
    pkt->fwd_cnt = 0;

    memset(payload, 0x42, 32);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(pkt), sizeof(*pkt),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(payload), 32, 0, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_REQUIRES(V0076, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_dgram_rw,
              "DGRAM connectionless data send",
              VIRTIO_SPEC_V1_4, "5.10.6",
              (1ULL << VIRTIO_VSOCK_F_DGRAM), 0);
