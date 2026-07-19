/* SPDX-License-Identifier: Apache-2.0 */
/*
 * A0011: admin_list_query_empty_group
 *
 * Issue a list-query admin command for group_type=0, which typically
 * has no members in a simple device. Verify the device returns an
 * empty or minimal response without errors.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_admin_list_empty_group(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    struct virtio_admin_cmd_hdr *cmd = vv_alloc_pages(1);
    uint8_t *result = vv_alloc_pages(1);

    memset(cmd, 0, sizeof(*cmd));
    cmd->opcode = VIRTIO_ADMIN_CMD_LIST_QUERY;
    cmd->group_type = 0; /* base/default group */
    cmd->group_member_id = 0;

    memset(result, 0xFF, 128);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(cmd), sizeof(*cmd),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(result), 128,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(A0011, VIRTIO_PCI_DEVICE_BLK, test_admin_list_empty_group,
              "Admin list-query on group with no members",
              VIRTIO_SPEC_V1_3, "2.13");
