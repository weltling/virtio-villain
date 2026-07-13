/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0072: dgram path is independent of stream credit tracking.
 *
 * v1.4 5.10.6 plus VIRTIO_VSOCK_F_DGRAM (bit 3): when negotiated,
 * datagrams flow on the same tx/rx queues but with type=3.
 * Send a dgram and verify credit fields are ignored.
 */
#include "tests/test.h"
#include "lib/virtio_spec.h"
#include "lib/util.h"

#include <string.h>

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_VSOCK_F_DGRAM)))
        return TEST_SKIP;

    struct virtio_vsock_hdr *h = vv_alloc_pages(1);
    memset(h, 0, sizeof(*h));
    h->src_cid = 3; h->dst_cid = 2;
    h->src_port = 1234; h->dst_port = 5678;
    h->len = 0;
    h->type = VIRTIO_VSOCK_TYPE_DGRAM;
    h->op = VIRTIO_VSOCK_OP_RW;
    h->buf_alloc = 0; h->fwd_cnt = 0;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(h), sizeof(*h), 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_REQUIRES(V0072, VIRTIO_PCI_DEVICE_VSOCK, test,
              "DGRAM with zero credit fields",
              VIRTIO_SPEC_V1_4, "5.10.6",
              (1ULL << VIRTIO_VSOCK_F_DGRAM), 0);
