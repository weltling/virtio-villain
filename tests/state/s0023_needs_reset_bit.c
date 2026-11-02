/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0023: needs_reset_bit_detection
 *
 * Drive the device into an error state (by submitting a clearly
 * invalid request and checking if NEEDS_RESET is signaled), or
 * verify the bit is not spuriously set during normal operation.
 */
#include "lib/virtio_test.h"

static test_result_t test_needs_reset_detection(struct virtio_dev *dev,
                                                struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* First verify NEEDS_RESET is NOT set during normal operation */
    __sync_synchronize();
    uint8_t status = cfg->device_status;
    if (status & VIRTIO_STATUS_NEEDS_RESET)
        TFAIL("status & VIRTIO_STATUS_NEEDS_RESET"); /* spuriously set before any error */

    /* Do a valid I/O to confirm the device is healthy */
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    *st = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, st, 1, VRING_DESC_F_WRITE, 0);

    VV_TRY(vv_submit_and_wait(dev, vr, 0));

    /* After successful I/O, NEEDS_RESET should still be clear */
    __sync_synchronize();
    status = cfg->device_status;
    if (status & VIRTIO_STATUS_NEEDS_RESET)
        TFAIL("status & VIRTIO_STATUS_NEEDS_RESET");

    return TEST_PASS;
}

REGISTER_TEST(S0023, VIRTIO_PCI_DEVICE_BLK, test_needs_reset_detection,
              "NEEDS_RESET bit not set during normal operation",
              VIRTIO_SPEC_V1_2, "2.1.2");
