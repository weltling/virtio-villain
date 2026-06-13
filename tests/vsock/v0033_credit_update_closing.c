/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0033: vsock_credit_update_closing
 *
 * Send a CREDIT_UPDATE on a stream that was never opened (simulating
 * a credit update arriving for a connection in CLOSING state). Tests
 * device handling of credit updates for non-existent connections.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_vsock_credit_closing(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_vsock_hdr *pkt = vv_alloc_pages(1);

    /* Credit update for a port that was never connected */
    pkt->src_cid = 3;
    pkt->dst_cid = 2;
    pkt->src_port = 55555;
    pkt->dst_port = 55555;
    pkt->len = 0;
    pkt->type = VIRTIO_VSOCK_TYPE_STREAM;
    pkt->op = VIRTIO_VSOCK_OP_CREDIT_UPDATE;
    pkt->flags = 0;
    pkt->buf_alloc = 65536;
    pkt->fwd_cnt = 1000;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(pkt),
                       sizeof(*pkt), 0, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(V0033, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_credit_closing,
              "CREDIT_UPDATE for non-existent/closing connection",
              VIRTIO_SPEC_V1_2, "5.10.6.3");
