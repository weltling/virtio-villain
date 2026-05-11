/* SPDX-License-Identifier: Apache-2.0 */
/*
 * A0023: admin_legacy_cfg_read
 *
 * Issue a legacy common configuration read command via admin VQ.
 * Spec v1.3 9.4: VIRTIO_ADMIN_CMD_LEGACY_COMMON_CFG_READ allows
 * the owner to read legacy registers of a group member.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

#define VIRTIO_ADMIN_CMD_LIST_QUERY                0x0000
#define VIRTIO_ADMIN_CMD_LIST_USE                  0x0001
#define VIRTIO_ADMIN_CMD_LEGACY_COMMON_CFG_READ    0x0004

struct virtio_admin_cmd {
    uint16_t opcode;
    uint16_t group_type;
    uint64_t group_member_id;
} __attribute__((packed));

struct virtio_admin_cmd_legacy_cfg {
    uint8_t offset;
    uint8_t reserved[3];
} __attribute__((packed));

struct virtio_admin_cmd_status {
    uint16_t status;
    uint16_t status_qualifier;
} __attribute__((packed));

static test_result_t test_admin_legacy_cfg_read(struct virtio_dev *dev,
                                                struct vring *vr)
{
    struct virtio_admin_cmd *cmd = vv_alloc_pages(1);
    struct virtio_admin_cmd_legacy_cfg *lcfg =
        (struct virtio_admin_cmd_legacy_cfg *)((uint8_t *)cmd + sizeof(*cmd));
    uint8_t *result = vv_alloc_pages(1);

    cmd->opcode = VIRTIO_ADMIN_CMD_LEGACY_COMMON_CFG_READ;
    cmd->group_type = 1;  /* SR-IOV */
    cmd->group_member_id = 0;
    lcfg->offset = 0;  /* Read from offset 0 of legacy common cfg */
    memset(lcfg->reserved, 0, sizeof(lcfg->reserved));

    uint64_t cmd_phys = vv_virt_to_phys(cmd);
    uint64_t result_phys = vv_virt_to_phys(result);

    vring_raw_set_desc(vr, 0, cmd_phys, sizeof(*cmd) + sizeof(*lcfg),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, result_phys, 64,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(A0023, VIRTIO_PCI_DEVICE_BLK, test_admin_legacy_cfg_read,
              "Admin legacy common cfg read command",
              VIRTIO_SPEC_V1_3, "9.4");
