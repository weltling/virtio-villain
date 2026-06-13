/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0010: vsock_unknown_stream
 *
 * Send data on a stream that was never connected (no prior REQUEST/
 * RESPONSE handshake). The device should reject it.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_vsock_unknown_stream(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_vsock_hdr *pkt = vv_alloc_pages(1);
    uint8_t *payload = vv_alloc_pages(1);

    /* Send data on port 9999 which was never established */
    pkt->src_cid = 3;
    pkt->dst_cid = 2;
    pkt->src_port = 9999;
    pkt->dst_port = 9999;
    pkt->len = 64;
    pkt->type = VIRTIO_VSOCK_TYPE_STREAM;
    pkt->op = VIRTIO_VSOCK_OP_RW;
    pkt->flags = 0;
    pkt->buf_alloc = 4096;
    pkt->fwd_cnt = 0;

    memset(payload, 0xDD, 64);

    uint64_t pkt_phys = vv_virt_to_phys(pkt);
    uint64_t payload_phys = vv_virt_to_phys(payload);

    vring_raw_set_desc(vr, 0, pkt_phys, sizeof(*pkt),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, payload_phys, 64, 0, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(V0010, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_unknown_stream,
              "Data on unconnected stream",
              VIRTIO_SPEC_V1_2, "5.10.6");
