/* SPDX-License-Identifier: Apache-2.0 */
/*
 * A0015: Admin command with corrupt group_type + valid opcode (spec 2.13)
 *
 * Submit a valid admin opcode (LIST_QUERY) but with group_type set
 * to 0xFFFF (invalid). The device should reject gracefully.
 */
#include "lib/virtio_test.h"

static test_result_t test_admin_corrupt_group_type(struct virtio_dev *dev)
{
    struct vring avr;
    vring_alloc(&avr, 16);
    vring_attach(dev, &avr, 0);

    struct virtio_admin_cmd_hdr *cmd = vv_alloc_pages(1);
    uint8_t *result = vv_alloc_pages(1);

    cmd->opcode = VIRTIO_ADMIN_CMD_LIST_QUERY;
    cmd->group_type = 0xFFFF; /* invalid */
    cmd->group_member_id = 0xFFFFFFFFFFFFFFFFULL;

    memset(result, 0xFF, 64);

    vv_desc(&avr, 0, cmd, sizeof(*cmd), VRING_DESC_F_NEXT, 1);
    vv_desc(&avr, 1, result, 64, VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(&avr, 0, 0);
    vring_raw_set_avail_idx(&avr, 1);

    return vv_kick_and_wait(dev, &avr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_DEV(A0015, VIRTIO_PCI_DEVICE_BLK, test_admin_corrupt_group_type,
                  "Admin LIST_QUERY with group_type=0xFFFF and max member_id",
                  VIRTIO_SPEC_V1_3, "2.13");
