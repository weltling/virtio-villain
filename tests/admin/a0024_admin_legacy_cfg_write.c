/* SPDX-License-Identifier: Apache-2.0 */
/*
 * A0024: admin_legacy_cfg_write
 *
 * Issue a legacy common configuration write command via admin VQ.
 * Spec v1.3 2.13: VIRTIO_ADMIN_CMD_LEGACY_COMMON_CFG_WRITE allows
 * the owner to write legacy registers of a group member.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_admin_legacy_cfg_write(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    struct virtio_admin_cmd *cmd = vv_alloc_pages(1);
    struct virtio_admin_cmd_legacy_wr *wr =
        (struct virtio_admin_cmd_legacy_wr *)((uint8_t *)cmd + sizeof(*cmd));
    uint8_t *result = vv_alloc_pages(1);

    cmd->opcode = VIRTIO_ADMIN_CMD_LEGACY_COMMON_CFG_WRITE;
    cmd->group_type = 1;  /* SR-IOV */
    cmd->group_member_id = 0;
    wr->offset = 0;
    memset(wr->reserved, 0, sizeof(wr->reserved));
    memset(wr->data, 0xAA, sizeof(wr->data));

    uint64_t cmd_phys = vv_virt_to_phys(cmd);
    uint64_t result_phys = vv_virt_to_phys(result);

    vring_raw_set_desc(vr, 0, cmd_phys, sizeof(*cmd) + sizeof(*wr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, result_phys, 64,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(A0024, VIRTIO_PCI_DEVICE_BLK, test_admin_legacy_cfg_write,
              "Admin legacy common cfg write command",
              VIRTIO_SPEC_V1_3, "2.13");
