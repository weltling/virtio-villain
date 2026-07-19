/* SPDX-License-Identifier: Apache-2.0 */
/*
 * A0029: Three concurrent LIST_USE commands on the same admin queue.
 *
 * Spec 2.13: Push three identical LIST_USE chains in the same
 * avail batch and a single kick. The device must process them
 * without descriptor cross talk or duplicate completions.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_admin_three_inflight(struct virtio_dev *dev,
                                               struct vring *vr)
{
    (void)vr;

    struct vring avr;
    vring_alloc(&avr, 16);
    vring_attach(dev, &avr, 0);

    for (int i = 0; i < 3; i++) {
        struct virtio_admin_cmd_hdr *cmd = vv_alloc_pages(1);
        uint8_t *data = vv_alloc_pages(1);
        uint8_t *result = vv_alloc_pages(1);
        memset(cmd, 0, sizeof(*cmd));
        memset(data, 0, 64);
        memset(result, 0xFF, 64);

        cmd->opcode = VIRTIO_ADMIN_CMD_LIST_USE;
        cmd->group_type = 1;
        cmd->group_member_id = 0;

        uint16_t d0 = (uint16_t)(i * 3);
        vring_raw_set_desc(&avr, d0, vv_virt_to_phys(cmd), sizeof(*cmd),
                           VRING_DESC_F_NEXT, d0 + 1);
        vring_raw_set_desc(&avr, d0 + 1, vv_virt_to_phys(data), 64,
                           VRING_DESC_F_NEXT, d0 + 2);
        vring_raw_set_desc(&avr, d0 + 2, vv_virt_to_phys(result), 64,
                           VRING_DESC_F_WRITE, 0);
        vring_raw_set_avail(&avr, (uint16_t)i, d0);
    }
    vring_raw_set_avail_idx(&avr, 3);

    return vv_kick_and_wait_n(dev, &avr, 0, 3, VV_TIMEOUT_MS);
}

REGISTER_TEST(A0029, VIRTIO_PCI_DEVICE_BLK, test_admin_three_inflight,
              "Three concurrent LIST_USE in one batch",
              VIRTIO_SPEC_V1_3, "2.13");
