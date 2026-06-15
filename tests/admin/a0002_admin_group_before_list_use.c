/* SPDX-License-Identifier: Apache-2.0 */
/*
 * A0002: admin_group_before_list_use
 *
 * Send a group-type admin command before completing LIST_USE for
 * that group type. Spec v1.3 2.9: driver MUST NOT use any commands
 * for a given group type before sending LIST_USE with the correct list.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

/* Use an arbitrary group-scoped opcode without prior LIST_USE */
#define ADMIN_CMD_ARBITRARY 0x0010

static test_result_t test_admin_group_before_list_use(struct virtio_dev *dev,
                                                      struct vring *vr)
{
    struct virtio_admin_cmd *cmd = vv_alloc_pages(1);
    uint8_t *result = vv_alloc_pages(1);

    /* Group type 2 command without LIST_USE for group 2 */
    cmd->opcode = ADMIN_CMD_ARBITRARY;
    cmd->group_type = 2;
    cmd->group_member_id = 0;

    uint64_t cmd_phys = vv_virt_to_phys(cmd);
    uint64_t result_phys = vv_virt_to_phys(result);

    vring_raw_set_desc(vr, 0, cmd_phys, sizeof(*cmd),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, result_phys, 64,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(A0002, VIRTIO_PCI_DEVICE_BLK, test_admin_group_before_list_use,
              "Admin group command before LIST_USE",
              VIRTIO_SPEC_V1_3, "2.9");
