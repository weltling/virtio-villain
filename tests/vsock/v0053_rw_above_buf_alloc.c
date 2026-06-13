/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0053: vsock_rw_payload_above_buf_alloc
 *
 * Send an OP_RW packet whose payload size exceeds the negotiated
 * buf_alloc. Spec 5.10.6.4 makes this a credit violation. The
 * device must apply back pressure or close the stream rather than
 * silently overrunning the receive buffer.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_vsock_rw_above_buf_alloc(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    uint8_t *page = vv_alloc_pages(2);
    struct virtio_vsock_hdr *pkt = (void *)page;

    pkt->src_cid = 3;
    pkt->dst_cid = 2;
    pkt->src_port = 5001;
    pkt->dst_port = 6001;
    pkt->len = 4096;
    pkt->type = VIRTIO_VSOCK_TYPE_STREAM;
    pkt->op = VIRTIO_VSOCK_OP_RW;
    pkt->flags = 0;
    pkt->buf_alloc = 64;
    pkt->fwd_cnt = 0;

    memset(page + sizeof(*pkt), 0x55, 4096);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(page),
                       sizeof(*pkt) + 4096, 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(V0053, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_rw_above_buf_alloc,
              "OP_RW payload above negotiated buf_alloc",
              VIRTIO_SPEC_V1_2, "5.10.6.4");
