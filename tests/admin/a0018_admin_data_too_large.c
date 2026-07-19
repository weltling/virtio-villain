/* SPDX-License-Identifier: Apache-2.0 */
/*
 * A0018: Submit admin command with data_len larger than the available
 * descriptor buffer.
 *
 * Spec 9.4: The command header's data_len field claims more data
 * than the descriptor's len provides. The device must handle this
 * mismatch without crashing.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_admin_data_too_large(struct virtio_dev *dev,
                                               struct vring *vr)
{
    (void)vr;

    struct vring avr;
    vring_alloc(&avr, 16);
    vring_attach(dev, &avr, 0);

    struct virtio_admin_cmd_hdr_short *cmd = vv_alloc_pages(1);
    uint8_t *result = vv_alloc_pages(1);

    memset(cmd, 0, PAGE_SIZE);
    memset(result, 0xFF, 64);

    /* Opcode 0 (list use), with data_len claiming 4096 bytes */
    cmd->opcode = 0;
    cmd->group_type = 0;
    cmd->group = 0;
    cmd->command_specific_data_len = 4096;

    /*
     * Command descriptor only provides sizeof(hdr) = 8 bytes,
     * but header claims 4096 bytes of command-specific data follow.
     */
    vring_raw_set_desc(&avr, 0, vv_virt_to_phys(cmd), sizeof(*cmd),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&avr, 1, vv_virt_to_phys(result), 64,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(&avr, 0, 0);
    vring_raw_set_avail_idx(&avr, 1);

    return vv_kick_and_wait(dev, &avr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(A0018, VIRTIO_PCI_DEVICE_BLK, test_admin_data_too_large,
              "Admin command data_len exceeds descriptor buffer size",
              VIRTIO_SPEC_V1_3, "2.13");
