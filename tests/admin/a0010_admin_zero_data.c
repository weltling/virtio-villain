/* SPDX-License-Identifier: Apache-2.0 */
/*
 * A0010: admin_cmd_zero_length_data
 *
 * Submit an admin command (list query) with a zero-length data
 * buffer. Tests device handling of degenerate buffer sizes in
 * admin command responses.
 */
#include "lib/virtio_test.h"

static test_result_t test_admin_zero_data(struct virtio_dev *dev,
                                          struct vring *vr)
{
    struct virtio_admin_cmd_hdr *cmd = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    cmd->opcode = VIRTIO_ADMIN_CMD_LIST_QUERY;
    cmd->group_type = 0;
    cmd->group_member_id = 0;

    *status = 0xFF;

    /* Header (readable) + status (writable, 1 byte only - no data buf) */
    vv_desc(vr, 0, cmd, sizeof(*cmd), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(A0010, VIRTIO_PCI_DEVICE_BLK, test_admin_zero_data,
              "Admin list-query with zero-length response buffer",
              VIRTIO_SPEC_V1_3, "2.13");
