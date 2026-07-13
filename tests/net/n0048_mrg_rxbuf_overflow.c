/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0048: net_mergeable_num_buffers_exceeds_posted
 *
 * With VIRTIO_NET_F_MRG_RXBUF, post a single small RX buffer, then
 * send a TX frame larger than that buffer. If the device tries to
 * merge across buffers that don't exist, it must handle gracefully.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_mrg_rxbuf_overflow(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* Check if MRG_RXBUF is available */
    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t offered = cfg->device_feature;
    if (!(offered & (1U << VIRTIO_NET_F_MRG_RXBUF)))
        return TEST_SKIP;

    if (cfg->num_queues < 2)
        return TEST_SKIP;

    /*
     * Post a single tiny RX buffer (64 bytes) on queue 0.
     * This is far too small for a full frame with mergeable headers.
     */
    uint8_t *rxbuf = vv_alloc_pages(1);
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(rxbuf), 64,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    __sync_synchronize();
    virtio_pci_kick(dev, 0);

    /* Set up TX queue (queue 1) */
    struct vring txvr;
    vring_alloc(&txvr, 16);
    vring_attach(dev, &txvr, 1);

    /* Send a TX frame larger than our RX buffer */
    struct virtio_net_hdr_mrg *txhdr = vv_alloc_pages(1);
    uint8_t *frame = vv_alloc_pages(1);

    memset(txhdr, 0, sizeof(*txhdr));
    memset(frame, 0xFF, 6);       /* dst broadcast */
    memset(frame + 6, 0x02, 6);   /* src */
    frame[12] = 0x08; frame[13] = 0x00;
    memset(frame + 14, 0xAA, 500);

    vring_raw_set_desc(&txvr, 0, vv_virt_to_phys(txhdr), sizeof(*txhdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&txvr, 1, vv_virt_to_phys(frame), 514, 0, 0);
    vring_raw_set_avail(&txvr, 0, 0);
    vring_raw_set_avail_idx(&txvr, 1);

    /* The TX should complete (device consumes it) */
    test_result_t r = vv_kick_and_wait(dev, &txvr, 1, VV_TIMEOUT_MS);

    /* Whether it passed or was rejected, device shouldn't crash */
    return r;
}

REGISTER_TEST_REQUIRES(N0048, VIRTIO_PCI_DEVICE_NET, test_net_mrg_rxbuf_overflow,
              "MRG_RXBUF TX larger than single posted RX buffer",
              VIRTIO_SPEC_V1_2, "5.1.6.4",
              (1ULL << VIRTIO_NET_F_MRG_RXBUF), 2);
