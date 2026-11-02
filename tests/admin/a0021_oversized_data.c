/* SPDX-License-Identifier: Apache-2.0 */
/*
 * A0021: admin_cmd_oversized_data
 *
 * Submit a LIST_USE command whose readable data buffer exceeds
 * one page. Spec 1.3 chapter 2.13 limits admin command data to
 * the negotiated maximum. The device must reject the command
 * without partial parse.
 */
#include "lib/virtio_test.h"

static test_result_t test_admin_oversized(struct virtio_dev *dev,
                                          struct vring *vr)
{
    struct virtio_admin_cmd_hdr *cmd = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(8);
    uint8_t *result = vv_alloc_pages(1);

    cmd->opcode = VIRTIO_ADMIN_CMD_LIST_USE;

    memset(data, 0xAA, 8 * 4096);

    vv_desc(vr, 0, cmd, sizeof(*cmd), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 8 * 4096, VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, result, 64, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(A0021, VIRTIO_PCI_DEVICE_BLK, test_admin_oversized,
              "Admin LIST_USE with 32 KiB readable data",
              VIRTIO_SPEC_V1_3, "2.13");
