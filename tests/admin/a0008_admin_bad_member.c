/* SPDX-License-Identifier: Apache-2.0 */
/*
 * A0008: admin_cmd_bad_group_member
 *
 * Submit an admin command with a group_member_id that doesn't exist.
 * Tests device validation of the target member before executing the
 * command operation.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_admin_cmd_hdr {
    uint16_t opcode;
    uint16_t group_type;
    uint8_t  reserved1[12];
    uint64_t group_member_id;
} __attribute__((packed));

#define VIRTIO_ADMIN_CMD_LIST_USE 0x0001

static test_result_t test_admin_bad_member(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_admin_cmd_hdr *cmd = vv_alloc_pages(1);
    uint8_t *result = vv_alloc_pages(1);

    memset(cmd, 0, sizeof(*cmd));
    cmd->opcode = VIRTIO_ADMIN_CMD_LIST_USE;
    cmd->group_type = 1; /* SR-IOV group type */
    cmd->group_member_id = 0xDEADBEEFCAFEULL; /* non-existent member */

    *result = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(cmd), sizeof(*cmd),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(result), 64,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(A0008, VIRTIO_PCI_DEVICE_BLK, test_admin_bad_member,
              "Admin command targeting non-existent group member",
              VIRTIO_SPEC_V1_3, "9.4");
