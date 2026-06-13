/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0070: net_ctrl_concurrent_rx_tx
 *
 * Submit a control command while TX and RX are both active.
 * Tests that the device handles control virtqueue operations
 * concurrently with data path traffic.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_ctrl_concurrent(struct virtio_dev *dev,
                                              struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* Need RX(0), TX(1), and ctrl queue (typically queue 2 for 1 pair) */
    if (cfg->num_queues < 3)
        return TEST_SKIP;

    /* Set up RX queue (0) */
    struct vring rxvr;
    vring_alloc(&rxvr, 16);
    vring_attach(dev, &rxvr, 0);

    /* Set up TX queue (1) */
    struct vring txvr;
    vring_alloc(&txvr, 16);
    vring_attach(dev, &txvr, 1);

    /* Set up ctrl queue (2) */
    struct vring ctrlvr;
    vring_alloc(&ctrlvr, 16);
    vring_attach(dev, &ctrlvr, 2);

    /* Post RX buffer */
    uint8_t *rxbuf = vv_alloc_pages(1);
    memset(rxbuf, 0, 1526);
    vring_raw_set_desc(&rxvr, 0, vv_virt_to_phys(rxbuf), 1526,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&rxvr, 0, 0);
    vring_raw_set_avail_idx(&rxvr, 1);

    /* Prepare TX packet */
    struct virtio_net_hdr *txhdr = vv_alloc_pages(1);
    uint8_t *frame = vv_alloc_pages(1);
    memset(txhdr, 0, sizeof(*txhdr));
    memset(frame, 0xFF, 6);
    memset(frame + 6, 0x02, 6);
    frame[12] = 0x08; frame[13] = 0x00;
    memset(frame + 14, 0xAA, 46);

    vring_raw_set_desc(&txvr, 0, vv_virt_to_phys(txhdr), sizeof(*txhdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&txvr, 1, vv_virt_to_phys(frame), 60, 0, 0);
    vring_raw_set_avail(&txvr, 0, 0);
    vring_raw_set_avail_idx(&txvr, 1);

    /* Prepare ctrl command: set promiscuous mode */
    struct virtio_net_ctrl_hdr *ctrl = vv_alloc_pages(1);
    uint8_t *ctrl_data = vv_alloc_pages(1);
    uint8_t *ctrl_ack = vv_alloc_pages(1);

    ctrl->class = VIRTIO_NET_CTRL_RX;
    ctrl->command = VIRTIO_NET_CTRL_RX_PROMISC;
    *ctrl_data = 1; /* enable */
    *ctrl_ack = 0xFF;

    vring_raw_set_desc(&ctrlvr, 0, vv_virt_to_phys(ctrl), sizeof(*ctrl),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&ctrlvr, 1, vv_virt_to_phys(ctrl_data), 1,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(&ctrlvr, 2, vv_virt_to_phys(ctrl_ack), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&ctrlvr, 0, 0);
    vring_raw_set_avail_idx(&ctrlvr, 1);

    /* Kick all three queues simultaneously */
    __sync_synchronize();
    virtio_pci_kick(dev, 0);
    virtio_pci_kick(dev, 1);
    virtio_pci_kick(dev, 2);

    /* Wait for at least TX and ctrl to complete */
    int elapsed = 0;
    int step = 10000;
    int tx_done = 0, ctrl_done = 0;
    while (elapsed < 1000000) {
        usleep(step);
        __sync_synchronize();
        if (!tx_done && txvr.used->idx != 0)
            tx_done = 1;
        if (!ctrl_done && ctrlvr.used->idx != 0)
            ctrl_done = 1;
        if (tx_done && ctrl_done)
            return TEST_PASS;
        elapsed += step;
    }

    __sync_synchronize();
    uint8_t status = cfg->device_status;
    if (status == 0)
        TWEDGED("status == 0");
    TREJECT("no device response within timeout");
}

REGISTER_TEST(N0070, VIRTIO_PCI_DEVICE_NET, test_net_ctrl_concurrent,
              "Control command concurrent with active RX and TX",
              VIRTIO_SPEC_V1_2, "5.1.6.5");
