/* SPDX-License-Identifier: Apache-2.0 */
/*
 * A0034: Admin LIST_USE with status descriptor before data.
 *
 * Spec 9.4: An admin command chain is laid out as readable
 * command header followed by optional readable command data
 * and a writable status/result. Build a chain whose writable
 * status descriptor sits between the readable header and the
 * readable data descriptor. A device that walks the chain
 * assuming a fixed shape can underrun the data or overflow
 * the writable region. The device must validate the chain
 * shape and reject the malformed order.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_admin_status_before_data(struct virtio_dev *dev,
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

    cmd->opcode     = VIRTIO_ADMIN_CMD_LIST_USE;
    cmd->group_type = 1;

    vring_raw_set_desc(&avr, 0, vv_virt_to_phys(cmd), sizeof(*cmd),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&avr, 1, vv_virt_to_phys(result), 64,
                       VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(&avr, 2, vv_virt_to_phys(data), 64,
                       VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(&avr, 3, vv_virt_to_phys(result) + 64, 16,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&avr, 0, 0);
    vring_raw_set_avail_idx(&avr, 1);

    return vv_kick_and_wait(dev, &avr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(A0034, VIRTIO_PCI_DEVICE_BLK,
              test_admin_status_before_data,
              "LIST_USE with status descriptor before data",
              VIRTIO_SPEC_V1_3, "2.13");
