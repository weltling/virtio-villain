/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0087: net_ctrl_mac_table_empty
 *
 * Submit CTRL_MAC_TABLE_SET with both entries arrays of length zero.
 * Spec 5.1.6.5 requires the device to atomically replace the filter
 * table. Empty arrays must clear it without use after free.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_ctrl_mac_table_empty(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    uint8_t *page = vv_alloc_pages(1);
    struct virtio_net_ctrl_hdr *hdr = (void *)page;
    uint32_t *uni_count = (uint32_t *)(page + sizeof(*hdr));
    uint32_t *mul_count = (uint32_t *)(page + sizeof(*hdr) + 4);
    uint8_t *status = vv_alloc_pages(1);

    hdr->class = VIRTIO_NET_CTRL_MAC;
    hdr->command = VIRTIO_NET_CTRL_MAC_TABLE_SET;
    *uni_count = 0;
    *mul_count = 0;
    *status = 0xFF;

    uint64_t base = vv_virt_to_phys(page);

    vring_raw_set_desc(vr, 0, base, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, base + sizeof(*hdr), 4,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, base + sizeof(*hdr) + 4, 4,
                       VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, vv_virt_to_phys(status), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(N0087, VIRTIO_PCI_DEVICE_NET, test_net_ctrl_mac_table_empty,
                "CTRL_MAC_TABLE_SET with empty unicast and multicast",
                VIRTIO_SPEC_V1_2, "5.1.6.5", VV_QUEUE_LAST);
