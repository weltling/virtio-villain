/* SPDX-License-Identifier: Apache-2.0 */
/*
 * A0013: Admin command response buffer overflow (spec 9.4)
 *
 * Submit an admin command with a response buffer too small to hold
 * the expected result. The device must handle buffer truncation
 * without crashing.
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

static test_result_t test_admin_response_overflow(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    (void)vr;

    struct vring avr;
    vring_alloc(&avr, 16);
    vring_attach(dev, &avr, 0);

    struct virtio_admin_cmd_hdr *cmd = vv_alloc_pages(1);
    uint8_t *result = vv_alloc_pages(1);

    memset(cmd, 0, sizeof(*cmd));
    cmd->opcode = VIRTIO_ADMIN_CMD_LIST_QUERY;
    cmd->group_type = 0;
    cmd->group_member_id = 0;

    memset(result, 0xFF, PAGE_SIZE);

    /* Provide only 1 byte for the response (too small) */
    vring_raw_set_desc(&avr, 0, vv_virt_to_phys(cmd), sizeof(*cmd),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&avr, 1, vv_virt_to_phys(result), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(&avr, 0, 0);
    vring_raw_set_avail_idx(&avr, 1);

    return vv_kick_and_wait(dev, &avr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(A0013, VIRTIO_PCI_DEVICE_BLK, test_admin_response_overflow,
              "Admin command with 1-byte response buffer (overflow)",
              VIRTIO_SPEC_V1_3, "9.4");
