/* SPDX-License-Identifier: Apache-2.0 */
/*
 * A0006: admin_cmd_status_buffer_tiny
 *
 * Send an admin command with a valid command header but a status/result
 * buffer of only 1 byte. The device needs to write at least a status
 * code (typically 2+ bytes). This tests whether the device validates
 * the writable buffer length before writing the result.
 */
#include "lib/virtio_test.h"

static test_result_t test_admin_status_tiny(struct virtio_dev *dev,
                                            struct vring *vr)
{
    struct virtio_admin_cmd_hdr *cmd = vv_alloc_pages(1);
    uint8_t *result = vv_alloc_pages(1);

    cmd->opcode = VIRTIO_ADMIN_CMD_LIST_USE;
    cmd->group_type = 0;
    cmd->group_member_id = 0;

    /* Full command header (readable) */
    vv_desc(vr, 0, cmd, sizeof(*cmd), VRING_DESC_F_NEXT, 1);
    /* Tiny result buffer - only 1 byte (device needs more) */
    vv_desc(vr, 1, result, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(A0006, VIRTIO_PCI_DEVICE_BLK, test_admin_status_tiny,
              "Admin command with 1-byte status buffer (too small)",
              VIRTIO_SPEC_V1_3, "2.13");
