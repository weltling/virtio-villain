/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0002: vsock_bad_op
 *
 * Submit a vsock packet with an unknown operation type.
 * The device must reject rather than crashing on unrecognized ops.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_vsock_bad_op(struct virtio_dev *dev,
                                       struct vring *vr)
{
    struct virtio_vsock_hdr *pkt = vv_alloc_pages(1);

    pkt->src_cid = 3;
    pkt->dst_cid = 2;
    pkt->src_port = 1234;
    pkt->dst_port = 5678;
    pkt->len = 0;
    pkt->type = VIRTIO_VSOCK_TYPE_STREAM;
    pkt->op = 0xFFFF; /* unknown operation */
    pkt->flags = 0;
    pkt->buf_alloc = 4096;
    pkt->fwd_cnt = 0;

    uint64_t pkt_phys = vv_virt_to_phys(pkt);

    vring_raw_set_desc(vr, 0, pkt_phys, sizeof(*pkt), 0, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(V0002, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_bad_op,
              "Unknown vsock operation type",
              VIRTIO_SPEC_V1_2, "5.10.6");
