/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0128: CTRL VLAN_DEL with an unknown VID.
 *
 * v1.4 5.1.4: CTRL class VIRTIO_NET_CTRL_VLAN command DEL
 * removes a VID from the filter. Removing a VID that was
 * never added must complete (ack OK) or fail gracefully.
 */
#include "tests/test.h"
#include "lib/util.h"

#include <string.h>

#define VIRTIO_NET_F_CTRL_VLAN 19
#define VIRTIO_NET_CTRL_VLAN   2
#define VIRTIO_NET_CTRL_VLAN_DEL 1

struct ctrl_hdr { uint8_t class_; uint8_t command; } __attribute__((packed));

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_NET_F_CTRL_VLAN)))
        return TEST_SKIP;

    struct ctrl_hdr *h = vv_alloc_pages(1);
    uint16_t *vid = vv_alloc_pages(1);
    uint8_t *ack = vv_alloc_pages(1);
    h->class_ = VIRTIO_NET_CTRL_VLAN;
    h->command = VIRTIO_NET_CTRL_VLAN_DEL;
    *vid = 0x0FFE;
    *ack = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(h), sizeof(*h),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(vid), sizeof(*vid),
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(ack), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(N0128, VIRTIO_PCI_DEVICE_NET, test,
                "VLAN_DEL with VID that was never added",
                VIRTIO_SPEC_V1_4, "5.1.4", VV_QUEUE_LAST);
