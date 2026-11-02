/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0027: NEEDS_RESET device-initiated (spec 2.1.2)
 *
 * After normal init, check that NEEDS_RESET is NOT set. Then
 * submit a potentially problematic request and verify the device
 * doesn't spuriously set NEEDS_RESET during normal operation.
 */
#include "lib/virtio_test.h"

static test_result_t test_needs_reset_not_spurious(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* After init, NEEDS_RESET must not be set */
    uint8_t status = cfg->device_status;
    if (status & VIRTIO_STATUS_NEEDS_RESET)
        TFAIL("status & VIRTIO_STATUS_NEEDS_RESET");

    /* Submit a valid read request */
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    *st = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, st, 1, VRING_DESC_F_WRITE, 0);
    test_result_t r = vv_submit_and_wait(dev, vr, 0);

    /* After a valid request, NEEDS_RESET should still not be set */
    status = cfg->device_status;
    if (status & VIRTIO_STATUS_NEEDS_RESET)
        TFAIL("status & VIRTIO_STATUS_NEEDS_RESET");

    /* If the read succeeded, double check status byte */
    if (r == TEST_PASS && *st != 0)
        TFAIL("r == TEST_PASS && *st != 0"); /* bad status despite used ring advance */

    return TEST_PASS;
}

REGISTER_TEST(S0027, VIRTIO_PCI_DEVICE_BLK, test_needs_reset_not_spurious,
              "NEEDS_RESET not spuriously set during normal I/O",
              VIRTIO_SPEC_V1_2, "2.1.2");
