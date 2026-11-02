/* SPDX-License-Identifier: Apache-2.0 */
/*
 * A0026: admin_legacy_dev_cfg_read
 *
 * Issue a legacy device configuration read command via admin VQ.
 * Spec v1.3 2.13: VIRTIO_ADMIN_CMD_LEGACY_DEV_CFG_READ accesses
 * the device specific legacy configuration space of a member.
 */
#include "lib/virtio_test.h"

static test_result_t test_admin_legacy_dev_read(struct virtio_dev *dev,
                                                struct vring *vr)
{
    struct virtio_admin_cmd *cmd = vv_alloc_pages(1);
    struct virtio_admin_cmd_legacy_cfg *lcfg =
        (struct virtio_admin_cmd_legacy_cfg *)((uint8_t *)cmd + sizeof(*cmd));
    uint8_t *result = vv_alloc_pages(1);

    cmd->opcode = VIRTIO_ADMIN_CMD_LEGACY_DEV_CFG_READ;
    cmd->group_type = 1;
    cmd->group_member_id = 0;
    lcfg->offset = 0;  /* Read device config at offset 0 */
    memset(lcfg->reserved, 0, sizeof(lcfg->reserved));

    uint64_t cmd_phys = vv_virt_to_phys(cmd);

    vring_raw_set_desc(vr, 0, cmd_phys, sizeof(*cmd) + sizeof(*lcfg),
                       VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, result, 64, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(A0026, VIRTIO_PCI_DEVICE_BLK, test_admin_legacy_dev_read,
              "Admin legacy device cfg read command",
              VIRTIO_SPEC_V1_3, "2.13");
