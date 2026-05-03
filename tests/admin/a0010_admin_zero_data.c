/* SPDX-License-Identifier: Apache-2.0 */
/*
 * A0010: admin_cmd_zero_length_data
 *
 * Submit an admin command (list query) with a zero-length data
 * buffer. Tests device handling of degenerate buffer sizes in
 * admin command responses.
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

#define VIRTIO_ADMIN_CMD_LIST_QUERY 0x0000

static test_result_t test_admin_zero_data(struct virtio_dev *dev,
                                          struct vring *vr)
{
    struct virtio_admin_cmd_hdr *cmd = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    memset(cmd, 0, sizeof(*cmd));
    cmd->opcode = VIRTIO_ADMIN_CMD_LIST_QUERY;
    cmd->group_type = 0;
    cmd->group_member_id = 0;

    *status = 0xFF;

    /* Header (readable) + status (writable, 1 byte only - no data buf) */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(cmd), sizeof(*cmd),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(status), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(A0010, VIRTIO_PCI_DEVICE_BLK, test_admin_zero_data,
              "Admin list-query with zero-length response buffer",
              VIRTIO_SPEC_V1_3, "9.4");
