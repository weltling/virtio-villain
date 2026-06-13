/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0030: vsock_connect_flood
 *
 * Send many CONNECT requests to different ports in a single avail
 * ring batch (one kick). Tests device handling of rapid connection
 * establishment under load - potential hash table/allocation pressure.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

#define NUM_CONNECTS 32

static test_result_t test_vsock_connect_flood(struct virtio_dev *dev,
                                              struct vring *vr)
{
    /* Allocate a page per packet */
    struct virtio_vsock_hdr *pkts[NUM_CONNECTS];
    for (int i = 0; i < NUM_CONNECTS; i++) {
        pkts[i] = vv_alloc_pages(1);
        pkts[i]->src_cid = 3;
        pkts[i]->dst_cid = 2;
        pkts[i]->src_port = 10000 + i;
        pkts[i]->dst_port = 10000 + i;
        pkts[i]->len = 0;
        pkts[i]->type = VIRTIO_VSOCK_TYPE_STREAM;
        pkts[i]->op = VIRTIO_VSOCK_OP_REQUEST;
        pkts[i]->flags = 0;
        pkts[i]->buf_alloc = 4096;
        pkts[i]->fwd_cnt = 0;
    }

    /* Set up descriptors and avail entries */
    uint16_t qsz = vr->size;
    uint16_t count = NUM_CONNECTS < qsz ? NUM_CONNECTS : qsz;

    for (uint16_t i = 0; i < count; i++) {
        vring_raw_set_desc(vr, i, vv_virt_to_phys(pkts[i]),
                           sizeof(struct virtio_vsock_hdr), 0, 0);
        vring_raw_set_avail(vr, i, i);
    }
    vring_raw_set_avail_idx(vr, count);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(V0030, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_connect_flood,
              "CONNECT flood - 32 connections in single batch",
              VIRTIO_SPEC_V1_2, "5.10.6");
