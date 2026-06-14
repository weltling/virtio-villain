/* SPDX-License-Identifier: Apache-2.0 */
/*
 * A0031: LIST_USE with group_member_id = 0xFFFFFFFFFFFFFFFF.
 *
 * Spec 9.4: group_member_id is the 64 bit identifier of the
 * group member targeted by an admin command. Send LIST_USE
 * with the maximum representable value. The device must
 * reject the unknown identifier cleanly rather than truncating
 * or sign extending into internal bookkeeping.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

#define VIRTIO_ADMIN_CMD_LIST_USE 0x0001

static test_result_t test_admin_member_id_max(struct virtio_dev *dev,
                                              struct vring *vr)
{
    (void)vr;

    struct vring avr;
    vring_alloc(&avr, 16);
    vring_attach(dev, &avr, 0);

    struct virtio_admin_cmd_hdr *cmd = vv_alloc_pages(1);
    uint8_t *data   = vv_alloc_pages(1);
    uint8_t *result = vv_alloc_pages(1);
    memset(cmd, 0, sizeof(*cmd));
    memset(data, 0, 64);
    memset(result, 0xFF, 64);

    cmd->opcode          = VIRTIO_ADMIN_CMD_LIST_USE;
    cmd->group_type      = 1;
    cmd->group_member_id = 0xFFFFFFFFFFFFFFFFULL;

    vring_raw_set_desc(&avr, 0, vv_virt_to_phys(cmd), sizeof(*cmd),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&avr, 1, vv_virt_to_phys(data), 64,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(&avr, 2, vv_virt_to_phys(result), 64,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&avr, 0, 0);
    vring_raw_set_avail_idx(&avr, 1);

    return vv_kick_and_wait(dev, &avr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(A0031, VIRTIO_PCI_DEVICE_BLK, test_admin_member_id_max,
              "Admin LIST_USE with group_member_id = max u64",
              VIRTIO_SPEC_V1_3, "9.4");
