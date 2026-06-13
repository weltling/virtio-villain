/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0036: vsock_seqpacket_zero_length_record
 *
 * With VIRTIO_VSOCK_F_SEQPACKET negotiated, send a SEQPACKET data
 * message with len=0 (zero-length record boundary). The device must
 * handle empty records gracefully.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

#define VSOCK_FLAGS_EOR             4  /* end of record */

static test_result_t test_vsock_seqpacket_zero_len(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* Check SEQPACKET feature */
    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t offered = cfg->device_feature;
    if (!(offered & (1U << VIRTIO_VSOCK_F_SEQPACKET)))
        return TEST_SKIP;

    if (cfg->num_queues < 2)
        return TEST_SKIP;

    /* TX on queue 1 */
    struct vring txvr;
    vring_alloc(&txvr, 16);
    vring_attach(dev, &txvr, 1);

    struct virtio_vsock_hdr *pkt = vv_alloc_pages(1);
    pkt->src_cid = 3;
    pkt->dst_cid = 2;
    pkt->src_port = 6000;
    pkt->dst_port = 6000;
    pkt->len = 0; /* zero-length record */
    pkt->type = VIRTIO_VSOCK_TYPE_SEQPACKET;
    pkt->op = VIRTIO_VSOCK_OP_RW;
    pkt->flags = VSOCK_FLAGS_EOR; /* end-of-record marker */
    pkt->buf_alloc = 4096;
    pkt->fwd_cnt = 0;

    vring_raw_set_desc(&txvr, 0, vv_virt_to_phys(pkt),
                       sizeof(*pkt), 0, 0);
    vring_raw_set_avail(&txvr, 0, 0);
    vring_raw_set_avail_idx(&txvr, 1);

    return vv_kick_and_wait(dev, &txvr, 1, VV_TIMEOUT_MS);
}

REGISTER_TEST(V0036, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_seqpacket_zero_len,
              "SEQPACKET zero-length record with EOR flag",
              VIRTIO_SPEC_V1_2, "5.10.6.3");
