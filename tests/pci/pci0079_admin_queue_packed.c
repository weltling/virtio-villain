/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0079: admin_queue_packed
 *
 * Set up the admin queue in packed ring mode and submit a command.
 * Spec v1.3 4.1.4.3.1 + 2.8: packed virtqueues are allowed for
 * admin queues when VIRTIO_F_RING_PACKED is negotiated.
 */
#include "lib/virtio_test.h"

static test_result_t test_pci_admin_packed(struct virtio_dev *dev,
                                           struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    if (dev->common_length < 0x48)
        return TEST_SKIP;

    volatile uint8_t *raw = (volatile uint8_t *)cfg;
    uint16_t admin_num = *(volatile uint16_t *)(raw + 0x46);
    if (admin_num == 0)
        return TEST_SKIP;

    struct virtio_admin_cmd *cmd = vv_alloc_pages(1);
    uint8_t *result = vv_alloc_pages(1);

    cmd->opcode = VIRTIO_ADMIN_CMD_LIST_QUERY;
    cmd->group_type = 1;
    cmd->group_member_id = 0;

    vv_desc(vr, 0, cmd, sizeof(*cmd), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, result, 64, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_PACKED(PCI0079, VIRTIO_PCI_DEVICE_BLK, test_pci_admin_packed,
                     "Admin queue with packed ring",
                     VIRTIO_SPEC_V1_3, "4.1.4.3.1");
