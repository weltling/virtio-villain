/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0027: vsock_credit_underflow
 *
 * Send a CREDIT_UPDATE with buf_alloc less than fwd_cnt. This creates
 * a situation where available credit (buf_alloc - fwd_cnt) would
 * underflow if calculated as unsigned. The device must handle this
 * gracefully without integer overflow bugs.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_vsock_credit_underflow(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    struct virtio_vsock_hdr *pkt = vv_alloc_pages(1);

    pkt->src_cid = 3;
    pkt->dst_cid = 2;
    pkt->src_port = 5000;
    pkt->dst_port = 5000;
    pkt->len = 0;
    pkt->type = VIRTIO_VSOCK_TYPE_STREAM;
    pkt->op = VIRTIO_VSOCK_OP_CREDIT_UPDATE;
    pkt->flags = 0;
    pkt->buf_alloc = 100;       /* small buffer */
    pkt->fwd_cnt = 0xFFFFFFF0;  /* huge fwd_cnt >> buf_alloc */

    uint64_t pkt_phys = vv_virt_to_phys(pkt);

    vring_raw_set_desc(vr, 0, pkt_phys, sizeof(*pkt), 0, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(V0027, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_credit_underflow,
              "CREDIT_UPDATE with buf_alloc < fwd_cnt (underflow)",
              VIRTIO_SPEC_V1_2, "5.10.6.3");
