/* SPDX-License-Identifier: Apache-2.0 */
/*
 * A0030: Single LIST_USE chain spanning the full admin queue.
 *
 * Spec 2.7.5.2: A descriptor chain may use every slot in the
 * queue. Submit one LIST_USE chain that occupies all 16 admin
 * queue descriptors. The first holds the header, the last is
 * device writable status, and intermediate slots are padding
 * data segments. The device must walk the long chain to
 * completion rather than truncating at an arbitrary limit.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_admin_full_queue_chain(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    (void)vr;

    struct vring avr;
    vring_alloc(&avr, 16);
    vring_attach(dev, &avr, 0);

    struct virtio_admin_cmd_hdr *cmd = vv_alloc_pages(1);
    uint8_t *pad = vv_alloc_pages(1);
    uint8_t *result = vv_alloc_pages(1);
    memset(cmd, 0, sizeof(*cmd));
    memset(pad, 0, 4096);
    memset(result, 0xFF, 64);

    cmd->opcode = VIRTIO_ADMIN_CMD_LIST_USE;
    cmd->group_type = 1;
    cmd->group_member_id = 0;

    /* Slot 0: header. Slots 1..14: 16 byte readable padding. Slot 15: writable status. */
    vring_raw_set_desc(&avr, 0, vv_virt_to_phys(cmd), sizeof(*cmd),
                       VRING_DESC_F_NEXT, 1);
    for (uint16_t i = 1; i < 15; i++) {
        vring_raw_set_desc(&avr, i, vv_virt_to_phys(pad) + i * 16, 16,
                           VRING_DESC_F_NEXT, (uint16_t)(i + 1));
    }
    vring_raw_set_desc(&avr, 15, vv_virt_to_phys(result), 64,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(&avr, 0, 0);
    vring_raw_set_avail_idx(&avr, 1);

    return vv_kick_and_wait(dev, &avr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(A0030, VIRTIO_PCI_DEVICE_BLK, test_admin_full_queue_chain,
              "Admin LIST_USE chain occupying all queue descriptors",
              VIRTIO_SPEC_V1_3, "2.7.5.2");
