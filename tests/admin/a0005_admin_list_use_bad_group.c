/* SPDX-License-Identifier: Apache-2.0 */
/*
 * A0005: admin_list_use_invalid_group
 *
 * Send VIRTIO_ADMIN_CMD_LIST_USE with an invalid group_type value.
 * The device should reject it rather than treating it as a valid
 * group type.
 */
#include "lib/virtio_test.h"

static test_result_t test_admin_list_use_bad_group(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    struct virtio_admin_cmd *cmd = vv_alloc_pages(1);
    uint8_t *result = vv_alloc_pages(1);

    cmd->opcode = VIRTIO_ADMIN_CMD_LIST_USE;
    cmd->group_type = 0xFFFF; /* invalid group type */
    cmd->group_member_id = 0;

    /* command header (device-readable) */
    vv_desc(vr, 0, cmd, sizeof(*cmd), VRING_DESC_F_NEXT, 1);
    /* result buffer (device-writable) */
    vv_desc(vr, 1, result, 64, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(A0005, VIRTIO_PCI_DEVICE_BLK, test_admin_list_use_bad_group,
              "LIST_USE with invalid group_type 0xFFFF",
              VIRTIO_SPEC_V1_3, "2.12");
