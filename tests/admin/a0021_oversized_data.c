/* SPDX-License-Identifier: Apache-2.0 */
/*
 * A0021: admin_cmd_oversized_data
 *
 * Submit a LIST_USE command whose readable data buffer exceeds
 * one page. Spec 1.3 chapter 9.4 limits admin command data to
 * the negotiated maximum. The device must reject the command
 * without partial parse.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

#define VIRTIO_ADMIN_CMD_LIST_USE 0x0001

static test_result_t test_admin_oversized(struct virtio_dev *dev,
                                          struct vring *vr)
{
    struct virtio_admin_cmd_hdr *cmd = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(8);
    uint8_t *result = vv_alloc_pages(1);

    memset(cmd, 0, sizeof(*cmd));
    cmd->opcode = VIRTIO_ADMIN_CMD_LIST_USE;

    memset(data, 0xAA, 8 * 4096);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(cmd), sizeof(*cmd),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 8 * 4096,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(result), 64,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(A0021, VIRTIO_PCI_DEVICE_BLK, test_admin_oversized,
              "Admin LIST_USE with 32 KiB readable data",
              VIRTIO_SPEC_V1_3, "9.4");
