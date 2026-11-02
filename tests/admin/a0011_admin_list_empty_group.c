/* SPDX-License-Identifier: Apache-2.0 */
/*
 * A0011: admin_list_query_empty_group
 *
 * Issue a list-query admin command for group_type=0, which typically
 * has no members in a simple device. Verify the device returns an
 * empty or minimal response without errors.
 */
#include "lib/virtio_test.h"

static test_result_t test_admin_list_empty_group(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    struct virtio_admin_cmd_hdr *cmd = vv_alloc_pages(1);
    uint8_t *result = vv_alloc_pages(1);

    cmd->opcode = VIRTIO_ADMIN_CMD_LIST_QUERY;
    cmd->group_type = 0; /* base/default group */
    cmd->group_member_id = 0;

    memset(result, 0xFF, 128);

    vv_desc(vr, 0, cmd, sizeof(*cmd), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, result, 128, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(A0011, VIRTIO_PCI_DEVICE_BLK, test_admin_list_empty_group,
              "Admin list-query on group with no members",
              VIRTIO_SPEC_V1_3, "2.13");
