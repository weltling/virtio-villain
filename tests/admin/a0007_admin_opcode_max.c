/* SPDX-License-Identifier: Apache-2.0 */
/*
 * A0007: admin_cmd_opcode_max
 *
 * Submit an admin command with opcode set to 0xFFFF (maximum undefined
 * value). Tests device handling of completely unknown admin commands
 * at the uint16 boundary.
 */
#include "lib/virtio_test.h"

static test_result_t test_admin_opcode_max(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_admin_cmd_hdr *cmd = vv_alloc_pages(1);
    uint8_t *result = vv_alloc_pages(1);

    cmd->opcode = 0xFFFF; /* maximum undefined opcode */
    cmd->group_type = 0;
    cmd->group_member_id = 0;

    *result = 0xFF;

    vv_desc(vr, 0, cmd, sizeof(*cmd), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, result, 64, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(A0007, VIRTIO_PCI_DEVICE_BLK, test_admin_opcode_max,
              "Admin command with opcode = 0xFFFF (max undefined)",
              VIRTIO_SPEC_V1_3, "2.13");
