/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0110: net_ctrl_vlan_id_4095
 *
 * Send CTRL_VLAN_ADD with VLAN ID 4095 (0xFFF). Spec 5.1.6.5.4
 * says valid VLAN IDs are 0 through 4094. ID 4095 is reserved by
 * IEEE 802.1Q. The device must reject or ignore it without
 * crashing.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_vlan_4095(struct virtio_dev *dev,
                                        struct vring *vr)
{
    uint8_t *page = vv_alloc_pages(1);
    memset(page, 0, 4096);

    struct virtio_net_ctrl_hdr *ctrl = (void *)page;
    ctrl->class = VIRTIO_NET_CTRL_VLAN;
    ctrl->command = VIRTIO_NET_CTRL_VLAN_ADD;

    uint16_t *vlan_id = (uint16_t *)(page + sizeof(*ctrl));
    *vlan_id = 4095;

    uint8_t *status = vv_alloc_pages(1);
    *status = 0xFF;

    uint64_t page_phys = vv_virt_to_phys(page);
    uint64_t status_phys = vv_virt_to_phys(status);

    vring_raw_set_desc(vr, 0, page_phys,
                       sizeof(*ctrl) + sizeof(uint16_t),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, VV_QUEUE_LAST, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(N0110, VIRTIO_PCI_DEVICE_NET, test_net_vlan_4095,
                "CTRL_VLAN_ADD with reserved VLAN ID 4095",
                VIRTIO_SPEC_V1_2, "5.1.6.5.4", VV_QUEUE_LAST);
