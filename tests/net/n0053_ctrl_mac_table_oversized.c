/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0053: CTRL_MAC_TABLE_SET with oversized table (spec 5.1.6.5.4)
 *
 * Send a CTRL_MAC command with MAC_TABLE_SET containing more
 * entries than would fit in a reasonable buffer, testing device
 * handling of oversized control data.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_ctrl_mac_table_oversized(struct virtio_dev *dev,
                                                       struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    if (cfg->num_queues < 3)
        return TEST_SKIP;

    /* Control queue is typically queue 2 for single-pair net */
    struct vring cvr;
    vring_alloc(&cvr, 16);
    vring_attach(dev, &cvr, 2);

    struct virtio_net_ctrl_hdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *ack = vv_alloc_pages(1);

    hdr->class = VIRTIO_NET_CTRL_MAC;
    hdr->command = VIRTIO_NET_CTRL_MAC_TABLE_SET;

    /* MAC table: unicast count (4096 = absurd) + garbage data */
    uint32_t *uc_count = (uint32_t *)data;
    *uc_count = 4096; /* way more than any device supports */
    memset(data + 4, 0xAA, PAGE_SIZE - 4);

    *ack = 0xFF;

    vring_raw_set_desc(&cvr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&cvr, 1, vv_virt_to_phys(data), PAGE_SIZE,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(&cvr, 2, vv_virt_to_phys(ack), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(&cvr, 0, 0);
    vring_raw_set_avail_idx(&cvr, 1);

    return vv_kick_and_wait(dev, &cvr, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST(N0053, VIRTIO_PCI_DEVICE_NET, test_net_ctrl_mac_table_oversized,
              "CTRL_MAC TABLE_SET with oversized entry count",
              VIRTIO_SPEC_V1_2, "5.1.6.5.4");
