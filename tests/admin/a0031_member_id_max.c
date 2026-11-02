/* SPDX-License-Identifier: Apache-2.0 */
/*
 * A0031: LIST_USE with group_member_id = 0xFFFFFFFFFFFFFFFF.
 *
 * Spec 2.13: group_member_id is the 64 bit identifier of the
 * group member targeted by an admin command. Send LIST_USE
 * with the maximum representable value. The device must
 * reject the unknown identifier cleanly rather than truncating
 * or sign extending into internal bookkeeping.
 */
#include "lib/virtio_test.h"

static test_result_t test_admin_member_id_max(struct virtio_dev *dev)
{
    struct vring avr;
    vring_alloc(&avr, 16);
    vring_attach(dev, &avr, 0);

    struct virtio_admin_cmd_hdr *cmd = vv_alloc_pages(1);
    uint8_t *data   = vv_alloc_pages(1);
    uint8_t *result = vv_alloc_pages(1);
    memset(data, 0, 64);
    memset(result, 0xFF, 64);

    cmd->opcode          = VIRTIO_ADMIN_CMD_LIST_USE;
    cmd->group_type      = 1;
    cmd->group_member_id = 0xFFFFFFFFFFFFFFFFULL;

    vv_desc(&avr, 0, cmd, sizeof(*cmd), VRING_DESC_F_NEXT, 1);
    vv_desc(&avr, 1, data, 64, VRING_DESC_F_NEXT, 2);
    vv_desc(&avr, 2, result, 64, VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&avr, 0, 0);
    vring_raw_set_avail_idx(&avr, 1);

    return vv_kick_and_wait(dev, &avr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_DEV(A0031, VIRTIO_PCI_DEVICE_BLK, test_admin_member_id_max,
                  "Admin LIST_USE with group_member_id = max u64",
                  VIRTIO_SPEC_V1_3, "2.13");
