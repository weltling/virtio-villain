/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0050: net_ctrl_rx_promisc_during_traffic
 *
 * Toggle promiscuous mode via the control virtqueue while a TX
 * request is in-flight on the data queue. Tests device handling
 * of concurrent control and data path operations.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_ctrl_promisc_during_tx(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t offered = cfg->device_feature;

    if (!(offered & (1U << VIRTIO_NET_F_CTRL_VQ)))
        return TEST_SKIP;
    if (!(offered & (1U << VIRTIO_NET_F_CTRL_RX)))
        return TEST_SKIP;

    /* Need at least RX(0) + TX(1) + CTRL(2) = 3 queues */
    if (cfg->num_queues < 3)
        return TEST_SKIP;

    /* TX on queue 1 (the default TX queue) */
    struct vring txvr;
    vring_alloc(&txvr, 16);
    vring_attach(dev, &txvr, 1);

    /* CTRL on the last queue */
    uint16_t ctrl_q = cfg->num_queues - 1;
    struct vring cvr;
    vring_alloc(&cvr, 16);
    vring_attach(dev, &cvr, ctrl_q);

    /* Submit TX */
    struct virtio_net_hdr *txhdr = vv_alloc_pages(1);
    uint8_t *frame = vv_alloc_pages(1);
    memset(txhdr, 0, sizeof(*txhdr));
    memset(frame, 0xFF, 6);
    memset(frame + 6, 0x02, 6);
    frame[12] = 0x08; frame[13] = 0x00;
    memset(frame + 14, 0xCC, 46);

    vring_raw_set_desc(&txvr, 0, vv_virt_to_phys(txhdr), sizeof(*txhdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&txvr, 1, vv_virt_to_phys(frame), 60, 0, 0);
    vring_raw_set_avail(&txvr, 0, 0);
    vring_raw_set_avail_idx(&txvr, 1);

    /* Submit CTRL promisc=ON at the same time */
    struct virtio_net_ctrl_hdr *chdr = vv_alloc_pages(1);
    uint8_t *on_off = vv_alloc_pages(1);
    uint8_t *ack = vv_alloc_pages(1);

    chdr->class = VIRTIO_NET_CTRL_RX;
    chdr->command = VIRTIO_NET_CTRL_RX_PROMISC;
    *on_off = 1; /* enable promisc */
    *ack = 0xFF;

    vring_raw_set_desc(&cvr, 0, vv_virt_to_phys(chdr), sizeof(*chdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&cvr, 1, vv_virt_to_phys(on_off), 1,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(&cvr, 2, vv_virt_to_phys(ack), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&cvr, 0, 0);
    vring_raw_set_avail_idx(&cvr, 1);

    /* Kick both simultaneously */
    __sync_synchronize();
    uint16_t before_tx = txvr.used->idx;
    uint16_t before_ctrl = cvr.used->idx;
    virtio_pci_kick(dev, 1);
    virtio_pci_kick(dev, ctrl_q);

    /* Wait for both */
    int elapsed = 0;
    int done_tx = 0, done_ctrl = 0;
    while (elapsed < VV_TIMEOUT_MS * 1000) {
        usleep(10000);
        __sync_synchronize();
        if (!done_tx && txvr.used->idx != before_tx)
            done_tx = 1;
        if (!done_ctrl && cvr.used->idx != before_ctrl)
            done_ctrl = 1;
        if (done_tx && done_ctrl)
            return TEST_PASS;
        elapsed += 10000;
    }

    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    TREJECT("no device response within timeout");
}

REGISTER_TEST(N0050, VIRTIO_PCI_DEVICE_NET, test_net_ctrl_promisc_during_tx,
              "CTRL_RX promisc toggle concurrent with TX traffic",
              VIRTIO_SPEC_V1_2, "5.1.6.5");
