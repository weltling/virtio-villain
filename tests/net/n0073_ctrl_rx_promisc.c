/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0073: net_ctrl_rx_promisc
 *
 * Send VIRTIO_NET_CTRL_RX PROMISC command via the control virtqueue.
 * Spec 5.1.6.5.1: Setting promisc mode causes the device to receive
 * all incoming packets regardless of destination MAC.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_ctrl_rx_promisc(struct virtio_dev *dev,
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
    if (cfg->num_queues < 3)
        return TEST_SKIP;

    uint16_t ctrl_q = (uint16_t)(cfg->num_queues - 1);
    struct vring ctrl_vr;
    vring_alloc(&ctrl_vr, 64);
    vring_attach(dev, &ctrl_vr, ctrl_q);

    struct virtio_net_ctrl_hdr *hdr = vv_alloc_pages(1);
    uint8_t *on_off = vv_alloc_pages(1);
    uint8_t *ack = vv_alloc_pages(1);

    hdr->class = VIRTIO_NET_CTRL_RX;
    hdr->command = VIRTIO_NET_CTRL_RX_PROMISC;
    *on_off = 1;  /* enable promisc */
    *ack = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t onoff_phys = vv_virt_to_phys(on_off);
    uint64_t ack_phys = vv_virt_to_phys(ack);

    vring_raw_set_desc(&ctrl_vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&ctrl_vr, 1, onoff_phys, 1,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(&ctrl_vr, 2, ack_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(&ctrl_vr, 0, 0);
    vring_raw_set_avail_idx(&ctrl_vr, 1);

    (void)vr;
    return vv_kick_and_wait(dev, &ctrl_vr, ctrl_q, VV_TIMEOUT_MS);
}

REGISTER_TEST_REQUIRES(N0073, VIRTIO_PCI_DEVICE_NET, test_net_ctrl_rx_promisc,
              "Control VQ: enable promiscuous mode",
              VIRTIO_SPEC_V1_2, "5.1.6.5.1",
              (1ULL << VIRTIO_NET_F_CTRL_RX) | (1ULL << VIRTIO_NET_F_CTRL_VQ), 0);
