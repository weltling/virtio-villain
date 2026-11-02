/* SPDX-License-Identifier: Apache-2.0 */
/*
 * A0003: admin_cmd_reserved_status
 *
 * Send an admin command with a reserved/invalid opcode and verify the
 * device handles it safely. Spec v1.3 2.12: driver MUST handle a
 * command error with a reserved status code gracefully.
 */
#include "lib/virtio_test.h"

/* Completely invalid opcode in the reserved range */
#define ADMIN_CMD_RESERVED 0xFFFF

static test_result_t test_admin_cmd_reserved_status(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    struct virtio_admin_cmd *cmd = vv_alloc_pages(1);
    uint8_t *result = vv_alloc_pages(1);

    /* Send command with reserved/invalid opcode */
    cmd->opcode = ADMIN_CMD_RESERVED;
    cmd->group_type = 0;
    cmd->group_member_id = 0;

    vv_desc(vr, 0, cmd, sizeof(*cmd), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, result, 64, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(A0003, VIRTIO_PCI_DEVICE_BLK, test_admin_cmd_reserved_status,
              "Admin command with reserved opcode",
              VIRTIO_SPEC_V1_3, "2.12");
