/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0054: CTRL_RX_ALLMULTI toggle (spec 5.1.6.5.1)
 *
 * Send CTRL_RX ALLMULTI command. This exercises the device control
 * path for multicast filtering. The device should process or reject
 * the command gracefully.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_ctrl_rx_allmulti(struct virtio_dev *dev,
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
    struct vring cvr;
    vring_alloc(&cvr, 16);
    vring_attach(dev, &cvr, ctrl_q);

    struct virtio_net_ctrl_hdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *ack = vv_alloc_pages(1);

    hdr->class = VIRTIO_NET_CTRL_RX;
    hdr->command = VIRTIO_NET_CTRL_RX_ALLMULTI;
    data[0] = 1; /* enable allmulti */
    *ack = 0xFF;

    vring_raw_set_desc(&cvr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&cvr, 1, vv_virt_to_phys(data), 1,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(&cvr, 2, vv_virt_to_phys(ack), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(&cvr, 0, 0);
    vring_raw_set_avail_idx(&cvr, 1);

    return vv_kick_and_wait(dev, &cvr, ctrl_q, VV_TIMEOUT_MS);
}

REGISTER_TEST_REQUIRES(N0054, VIRTIO_PCI_DEVICE_NET, test_net_ctrl_rx_allmulti,
              "CTRL_RX ALLMULTI toggle command",
              VIRTIO_SPEC_V1_2, "5.1.6.5.1",
              (1ULL << VIRTIO_NET_F_CTRL_RX) | (1ULL << VIRTIO_NET_F_CTRL_VQ), 0);
