/* SPDX-License-Identifier: Apache-2.0 */
/*
 * A0034: Admin LIST_USE with status descriptor before data.
 *
 * Spec 2.13: An admin command chain is laid out as readable
 * command header followed by optional readable command data
 * and a writable status/result. Build a chain whose writable
 * status descriptor sits between the readable header and the
 * readable data descriptor. A device that walks the chain
 * assuming a fixed shape can underrun the data or overflow
 * the writable region. The device must validate the chain
 * shape and reject the malformed order.
 */
#include "lib/virtio_test.h"

static test_result_t test_admin_status_before_data(struct virtio_dev *dev)
{
    struct vring avr;
    vring_alloc(&avr, 16);
    vring_attach(dev, &avr, 0);

    struct virtio_admin_cmd_hdr *cmd = vv_alloc_pages(1);
    uint8_t *data   = vv_alloc_pages(1);
    uint8_t *result = vv_alloc_pages(1);
    memset(data, 0, 64);
    memset(result, 0xFF, 64);

    cmd->opcode     = VIRTIO_ADMIN_CMD_LIST_USE;
    cmd->group_type = 1;

    vv_desc(&avr, 0, cmd, sizeof(*cmd), VRING_DESC_F_NEXT, 1);
    vv_desc(&avr, 1, result, 64, VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 2);
    vv_desc(&avr, 2, data, 64, VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(&avr, 3, vv_virt_to_phys(result) + 64, 16,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&avr, 0, 0);
    vring_raw_set_avail_idx(&avr, 1);

    return vv_kick_and_wait(dev, &avr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_DEV(A0034, VIRTIO_PCI_DEVICE_BLK,
                  test_admin_status_before_data,
                  "LIST_USE with status descriptor before data",
                  VIRTIO_SPEC_V1_3, "2.13");
