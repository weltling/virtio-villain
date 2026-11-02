/* SPDX-License-Identifier: Apache-2.0 */
/*
 * A0027: admin_legacy_cfg_wrong_group_type
 *
 * Issue a legacy common cfg read with group_type=0 (invalid).
 * Spec v1.3 2.13: only valid group types should be accepted.
 */
#include "lib/virtio_test.h"

static test_result_t test_admin_legacy_wrong_group(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    struct virtio_admin_cmd *cmd = vv_alloc_pages(1);
    struct virtio_admin_cmd_legacy_cfg *lcfg =
        (struct virtio_admin_cmd_legacy_cfg *)((uint8_t *)cmd + sizeof(*cmd));
    uint8_t *result = vv_alloc_pages(1);

    cmd->opcode = VIRTIO_ADMIN_CMD_LEGACY_COMMON_CFG_READ;
    cmd->group_type = 0;  /* Invalid group type */
    cmd->group_member_id = 0;
    lcfg->offset = 0;
    memset(lcfg->reserved, 0, sizeof(lcfg->reserved));

    uint64_t cmd_phys = vv_virt_to_phys(cmd);

    vring_raw_set_desc(vr, 0, cmd_phys, sizeof(*cmd) + sizeof(*lcfg),
                       VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, result, 64, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(A0027, VIRTIO_PCI_DEVICE_BLK, test_admin_legacy_wrong_group,
              "Admin legacy cfg read with invalid group type",
              VIRTIO_SPEC_V1_3, "2.13");
