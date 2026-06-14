/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0078: admin_queue_notify
 *
 * Set up the admin queue, post a descriptor, and write to the
 * notification register for the admin queue index. Spec v1.3
 * 4.1.4.4: the admin queue uses the same notification mechanism
 * as data queues.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

#define VIRTIO_ADMIN_CMD_LIST_QUERY 0x0000

static test_result_t test_pci_admin_queue_notify(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    if (dev->common_length < 0x48)
        return TEST_SKIP;

    volatile uint8_t *raw = (volatile uint8_t *)cfg;
    uint16_t admin_num = *(volatile uint16_t *)(raw + 0x46);
    if (admin_num == 0)
        return TEST_SKIP;

    /* Just attempt a LIST_QUERY on the admin queue to exercise
     * the notification path. The device may or may not support
     * admin commands, so we accept any completion. */
    struct virtio_admin_cmd *cmd = vv_alloc_pages(1);
    uint8_t *result = vv_alloc_pages(1);

    cmd->opcode = VIRTIO_ADMIN_CMD_LIST_QUERY;
    cmd->group_type = 1;
    cmd->group_member_id = 0;

    uint64_t cmd_phys = vv_virt_to_phys(cmd);
    uint64_t result_phys = vv_virt_to_phys(result);

    vring_raw_set_desc(vr, 0, cmd_phys, sizeof(*cmd),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, result_phys, 64,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(PCI0078, VIRTIO_PCI_DEVICE_BLK, test_pci_admin_queue_notify,
              "Notify admin queue and receive completion",
              VIRTIO_SPEC_V1_3, "4.1.4.4");
