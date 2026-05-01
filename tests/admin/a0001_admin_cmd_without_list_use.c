/* SPDX-License-Identifier: Apache-2.0 */
/*
 * A0001: admin_cmd_without_list_use
 *
 * Send an admin command on the admin virtqueue without first issuing
 * VIRTIO_ADMIN_CMD_LIST_USE. Spec v1.3 2.9: driver MUST issue
 * LIST_USE and wait for success before using any other admin commands.
 *
 * This test uses queue 0 of a block device to inject an admin command
 * structure. While the device may not have a real admin virtqueue,
 * we exercise the VMM's handling of unexpected admin-like payloads.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

/* Admin command opcodes (v1.3 section 2.9) */
#define VIRTIO_ADMIN_CMD_LIST_QUERY 0x0000
#define VIRTIO_ADMIN_CMD_LIST_USE   0x0001

/* A fabricated admin command header per v1.3 spec */
struct virtio_admin_cmd {
    uint16_t opcode;
    uint16_t group_type;
    uint64_t group_member_id;
} __attribute__((packed));

struct virtio_admin_cmd_status {
    uint16_t status;
    uint16_t status_qualifier;
} __attribute__((packed));

static test_result_t test_admin_cmd_without_list_use(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    struct virtio_admin_cmd *cmd = vv_alloc_pages(1);
    uint8_t *result = vv_alloc_pages(1);

    /*
     * Send LIST_QUERY directly (which is allowed) but frame it as
     * a random opcode (0x0005) without prior LIST_USE handshake.
     */
    cmd->opcode = 0x0005; /* arbitrary command, not LIST_QUERY/LIST_USE */
    cmd->group_type = 1;
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

REGISTER_TEST(A0001, VIRTIO_PCI_DEVICE_BLK, test_admin_cmd_without_list_use,
              "Admin command without prior LIST_USE",
              VIRTIO_SPEC_V1_3, "2.9");
