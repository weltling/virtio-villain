/* SPDX-License-Identifier: Apache-2.0 */
/*
 * A0005: admin_list_use_invalid_group
 *
 * Send VIRTIO_ADMIN_CMD_LIST_USE with an invalid group_type value.
 * The device should reject it rather than treating it as a valid
 * group type.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_admin_cmd {
    uint16_t opcode;
    uint16_t group_type;
    uint64_t group_member_id;
} __attribute__((packed));

#define VIRTIO_ADMIN_CMD_LIST_USE 0x0001

static test_result_t test_admin_list_use_bad_group(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    struct virtio_admin_cmd *cmd = vv_alloc_pages(1);
    uint8_t *result = vv_alloc_pages(1);

    cmd->opcode = VIRTIO_ADMIN_CMD_LIST_USE;
    cmd->group_type = 0xFFFF; /* invalid group type */
    cmd->group_member_id = 0;

    uint64_t cmd_phys = vv_virt_to_phys(cmd);
    uint64_t result_phys = vv_virt_to_phys(result);

    /* command header (device-readable) */
    vring_raw_set_desc(vr, 0, cmd_phys, sizeof(*cmd),
                       VRING_DESC_F_NEXT, 1);
    /* result buffer (device-writable) */
    vring_raw_set_desc(vr, 1, result_phys, 64,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(A0005, VIRTIO_PCI_DEVICE_BLK, test_admin_list_use_bad_group,
              "LIST_USE with invalid group_type 0xFFFF",
              VIRTIO_SPEC_V1_3, "2.9");
