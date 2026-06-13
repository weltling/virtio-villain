/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0005: vsock_data_beyond_credit
 *
 * Send more data than the peer advertised via buf_alloc.
 * The device must reject excess data rather than overflowing buffers.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_vsock_data_beyond_credit(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    /* Allocate header + large payload */
    struct virtio_vsock_hdr *pkt = vv_alloc_pages(1);
    uint8_t *payload = vv_alloc_pages(8); /* 32 KiB */

    pkt->src_cid = 3;
    pkt->dst_cid = 2;
    pkt->src_port = 1234;
    pkt->dst_port = 5678;
    /* Claim 32 KiB of data even though peer didn't offer credit */
    pkt->len = 32768;
    pkt->type = VIRTIO_VSOCK_TYPE_STREAM;
    pkt->op = VIRTIO_VSOCK_OP_RW;
    pkt->flags = 0;
    /* Advertise tiny credit from our side */
    pkt->buf_alloc = 64;
    pkt->fwd_cnt = 0;

    memset(payload, 0xCC, 32768);

    uint64_t pkt_phys = vv_virt_to_phys(pkt);
    uint64_t payload_phys = vv_virt_to_phys(payload);

    vring_raw_set_desc(vr, 0, pkt_phys, sizeof(*pkt),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, payload_phys, 32768, 0, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(V0005, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_data_beyond_credit,
              "Send data exceeding peer credit",
              VIRTIO_SPEC_V1_2, "5.10.6");
