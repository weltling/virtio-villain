/* SPDX-License-Identifier: Apache-2.0 */
/*
 * A0004: admin_cmd_truncated_data
 *
 * Send an admin command where the data buffer is too short - the
 * device-readable portion doesn't contain a full command header.
 * Tests that the device validates buffer lengths before parsing.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_admin_cmd_truncated(struct virtio_dev *dev,
                                              struct vring *vr)
{
    uint8_t *cmd = vv_alloc_pages(1);
    uint8_t *result = vv_alloc_pages(1);

    /* Write only 2 bytes - less than the minimum admin cmd header */
    cmd[0] = 0x01; /* partial opcode */
    cmd[1] = 0x00;

    uint64_t cmd_phys = vv_virt_to_phys(cmd);
    uint64_t result_phys = vv_virt_to_phys(result);

    /* Truncated command header: only 2 bytes instead of full header */
    vring_raw_set_desc(vr, 0, cmd_phys, 2,
                       VRING_DESC_F_NEXT, 1);
    /* result buffer (device-writable) */
    vring_raw_set_desc(vr, 1, result_phys, 64,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(A0004, VIRTIO_PCI_DEVICE_BLK, test_admin_cmd_truncated,
              "Admin command with truncated header buffer",
              VIRTIO_SPEC_V1_3, "2.9");
