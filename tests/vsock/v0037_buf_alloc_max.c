/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0037: vsock_buf_alloc_uint32_max
 *
 * Send a CONNECT request with buf_alloc set to UINT32_MAX, testing
 * device handling of maximum credit advertisement.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_vsock_buf_alloc_max(struct virtio_dev *dev,
                                              struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    if (cfg->num_queues < 2)
        return TEST_SKIP;

    /* TX on queue 1 */
    struct vring txvr;
    vring_alloc(&txvr, 16);
    vring_attach(dev, &txvr, 1);

    struct virtio_vsock_hdr *pkt = vv_alloc_pages(1);
    pkt->src_cid = 3;
    pkt->dst_cid = 2;
    pkt->src_port = 7000;
    pkt->dst_port = 7000;
    pkt->len = 0;
    pkt->type = VIRTIO_VSOCK_TYPE_STREAM;
    pkt->op = VIRTIO_VSOCK_OP_REQUEST;
    pkt->flags = 0;
    pkt->buf_alloc = 0xFFFFFFFF; /* UINT32_MAX */
    pkt->fwd_cnt = 0;

    vring_raw_set_desc(&txvr, 0, vv_virt_to_phys(pkt),
                       sizeof(*pkt), 0, 0);
    vring_raw_set_avail(&txvr, 0, 0);
    vring_raw_set_avail_idx(&txvr, 1);

    return vv_kick_and_wait(dev, &txvr, 1, VV_TIMEOUT_MS);
}

REGISTER_TEST_REQUIRES(V0037, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_buf_alloc_max,
              "CONNECT with buf_alloc=UINT32_MAX",
              VIRTIO_SPEC_V1_2, "5.10.6.2",
              0, 2);
