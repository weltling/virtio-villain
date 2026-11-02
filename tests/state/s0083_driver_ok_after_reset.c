/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0083: driver_ok_immediately_after_reset
 *
 * Reset the device and write DRIVER_OK as the very next status
 * value, skipping ACKNOWLEDGE, DRIVER, feature negotiation, and
 * queue setup entirely. Spec 3.1.1 requires steps 1 through 7
 * before step 8. This is the most extreme sequence violation.
 * The device must not crash or corrupt host memory, and must be able
 * to recover via a clean reset and handle valid requests afterwards.
 */
#include "lib/virtio_test.h"

static test_result_t test_driver_ok_after_reset(struct virtio_dev *dev,
                                                struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* Reset */
    virtio_pci_reset(dev);

    /* Write DRIVER_OK directly, skipping everything */
    cfg->device_status = VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();
    usleep(10000);

    /* Verify the device can cleanly recover via standard reset and init */
    if (virtio_pci_init(dev) < 0)
        TFAIL("reinit after sequence violation failed");

    vring_attach(dev, vr, 0);
    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    VV_TRY(vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS));
    if (*status != 0)
        TFAIL("Expected status to be VIRTIO_BLK_S_OK, found %u", *status);
    return TEST_PASS;
}

REGISTER_TEST(S0083, VIRTIO_PCI_DEVICE_BLK, test_driver_ok_after_reset,
              "DRIVER_OK immediately after reset",
              VIRTIO_SPEC_V1_2, "3.1.1");
