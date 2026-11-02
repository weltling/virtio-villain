/* SPDX-License-Identifier: Apache-2.0 */
/*
 * A0017: Admin command with zero-length command buffer (spec 2.13)
 *
 * Submit an admin command descriptor with len=0 for the command
 * header. This is an invalid request - the device must handle
 * it without crashing.
 */
#include "lib/virtio_test.h"

static test_result_t test_admin_zero_cmd_len(struct virtio_dev *dev)
{
    struct vring avr;
    vring_alloc(&avr, 16);
    vring_attach(dev, &avr, 0);

    uint8_t *cmd = vv_alloc_pages(1);
    uint8_t *result = vv_alloc_pages(1);

    memset(result, 0xFF, 64);

    /* Command descriptor with len=0 (invalid) */
    vv_desc(&avr, 0, cmd, 0, VRING_DESC_F_NEXT, 1);
    vv_desc(&avr, 1, result, 64, VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(&avr, 0, 0);
    vring_raw_set_avail_idx(&avr, 1);

    return vv_kick_and_wait(dev, &avr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_DEV(A0017, VIRTIO_PCI_DEVICE_BLK, test_admin_zero_cmd_len,
                  "Admin command with zero-length command descriptor",
                  VIRTIO_SPEC_V1_3, "2.13");
