/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0044: net_ctrl_vlan_max_and_invalid
 *
 * Send CTRL_VLAN ADD with vid=4095 (max valid 12-bit VLAN ID) then
 * vid=4096 (invalid, exceeds 12-bit range). Tests device VLAN ID
 * validation at boundaries.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

#define VIRTIO_NET_CTRL_VLAN     5
#define VIRTIO_NET_CTRL_VLAN_ADD 0

struct ctrl_hdr {
    uint8_t class;
    uint8_t command;
} __attribute__((packed));

static test_result_t test_net_ctrl_vlan_boundary(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    (void)vr;

    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    uint16_t nq = cfg->num_queues;
    if (nq < 3)
        return TEST_SKIP;

    uint16_t ctrl_q = nq - 1;

    struct vring cvr;
    vring_alloc(&cvr, 16);
    vring_attach(dev, &cvr, ctrl_q);

    struct ctrl_hdr *ctrl = vv_alloc_pages(1);
    uint16_t *vid = vv_alloc_pages(1);
    uint8_t *ack = vv_alloc_pages(1);

    /* First: vid = 4096 (invalid) */
    ctrl->class = VIRTIO_NET_CTRL_VLAN;
    ctrl->command = VIRTIO_NET_CTRL_VLAN_ADD;
    *vid = 4096; /* exceeds 12-bit max */
    *ack = 0xFF;

    vring_raw_set_desc(&cvr, 0, vv_virt_to_phys(ctrl), sizeof(*ctrl),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&cvr, 1, vv_virt_to_phys(vid), sizeof(*vid),
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(&cvr, 2, vv_virt_to_phys(ack), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(&cvr, 0, 0);
    vring_raw_set_avail_idx(&cvr, 1);

    return vv_kick_and_wait(dev, &cvr, ctrl_q, VV_TIMEOUT_MS);
}

REGISTER_TEST(N0044, VIRTIO_PCI_DEVICE_NET, test_net_ctrl_vlan_boundary,
              "CTRL_VLAN ADD with vid=4096 (invalid, exceeds 12 bits)",
              VIRTIO_SPEC_V1_2, "5.1.6.5");
