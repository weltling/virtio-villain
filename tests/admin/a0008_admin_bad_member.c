/* SPDX-License-Identifier: Apache-2.0 */
/*
 * A0008: admin_cmd_bad_group_member
 *
 * Submit an admin command with a group_member_id that doesn't exist.
 * Tests device validation of the target member before executing the
 * command operation.
 */
#include "lib/virtio_test.h"

static test_result_t test_admin_bad_member(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_admin_cmd_hdr *cmd = vv_alloc_pages(1);
    uint8_t *result = vv_alloc_pages(1);

    cmd->opcode = VIRTIO_ADMIN_CMD_LIST_USE;
    cmd->group_type = 1; /* SR-IOV group type */
    cmd->group_member_id = 0xDEADBEEFCAFEULL; /* non-existent member */

    *result = 0xFF;

    vv_desc(vr, 0, cmd, sizeof(*cmd), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, result, 64, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(A0008, VIRTIO_PCI_DEVICE_BLK, test_admin_bad_member,
              "Admin command targeting non-existent group member",
              VIRTIO_SPEC_V1_3, "2.13");
