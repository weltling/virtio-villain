/* SPDX-License-Identifier: Apache-2.0 */
/*
 * A0032: Admin LIST_USE with the command header split across two
 * readable descriptors.
 *
 * Spec 2.7.5.2: A descriptor chain may split a single logical
 * buffer across multiple descriptors. Submit LIST_USE where the
 * 24 byte command header straddles two readable descriptors,
 * with the data and writable result trailing. The device must
 * coalesce the readable region and parse the header normally.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_admin_header_split(struct virtio_dev *dev,
                                             struct vring *vr)
{
    (void)vr;

    struct vring avr;
    vring_alloc(&avr, 16);
    vring_attach(dev, &avr, 0);

    uint8_t *backing = vv_alloc_pages(1);
    memset(backing, 0, 4096);
    struct virtio_admin_cmd_hdr *cmd = (struct virtio_admin_cmd_hdr *)backing;
    cmd->opcode          = VIRTIO_ADMIN_CMD_LIST_USE;
    cmd->group_type      = 1;
    cmd->group_member_id = 0;

    uint8_t *data   = vv_alloc_pages(1);
    uint8_t *result = vv_alloc_pages(1);
    memset(data, 0, 64);
    memset(result, 0xFF, 64);

    uint64_t cmd_phys = vv_virt_to_phys(cmd);
    uint32_t first  = 8;
    uint32_t second = (uint32_t)sizeof(*cmd) - first;

    vring_raw_set_desc(&avr, 0, cmd_phys, first,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&avr, 1, cmd_phys + first, second,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(&avr, 2, vv_virt_to_phys(data), 64,
                       VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(&avr, 3, vv_virt_to_phys(result), 64,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&avr, 0, 0);
    vring_raw_set_avail_idx(&avr, 1);

    return vv_kick_and_wait(dev, &avr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(A0032, VIRTIO_PCI_DEVICE_BLK, test_admin_header_split,
              "LIST_USE header split across two readable descriptors",
              VIRTIO_SPEC_V1_3, "2.7.5.2");
