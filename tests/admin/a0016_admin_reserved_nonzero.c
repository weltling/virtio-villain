/* SPDX-License-Identifier: Apache-2.0 */
/*
 * A0016: Admin command with all reserved header bytes non-zero (spec 9.4)
 *
 * The admin cmd header has 12 reserved bytes. Set them all to 0xFF.
 * The device must ignore reserved fields without crashing.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

#define VIRTIO_ADMIN_CMD_LIST_QUERY 0x0000

static test_result_t test_admin_reserved_nonzero(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    (void)vr;

    struct vring avr;
    vring_alloc(&avr, 16);
    vring_attach(dev, &avr, 0);

    struct virtio_admin_cmd_hdr *cmd = vv_alloc_pages(1);
    uint8_t *result = vv_alloc_pages(1);

    /* Fill entire header with 0xFF then set valid fields */
    memset(cmd, 0xFF, sizeof(*cmd));
    cmd->opcode = VIRTIO_ADMIN_CMD_LIST_QUERY;
    cmd->group_type = 0;
    cmd->group_member_id = 0;
    /* reserved1[12] stays 0xFF */

    memset(result, 0xFF, 64);

    vring_raw_set_desc(&avr, 0, vv_virt_to_phys(cmd), sizeof(*cmd),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&avr, 1, vv_virt_to_phys(result), 64,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(&avr, 0, 0);
    vring_raw_set_avail_idx(&avr, 1);

    return vv_kick_and_wait(dev, &avr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(A0016, VIRTIO_PCI_DEVICE_BLK, test_admin_reserved_nonzero,
              "Admin command with all reserved header bytes set to 0xFF",
              VIRTIO_SPEC_V1_3, "9.4");
