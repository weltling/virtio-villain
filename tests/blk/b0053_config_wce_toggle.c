/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0053: config_wce_toggle
 *
 * Toggle the writeback/writethrough cache mode via the WCE
 * (Write Cache Enable) config field if VIRTIO_BLK_F_CONFIG_WCE
 * is offered. Write 0 then 1 and verify the device accepts both.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_config_wce_toggle(struct virtio_dev *dev,
                                                struct vring *vr)
{
    if (!dev->device_cfg)
        return TEST_SKIP;

    /* Check if device offers CONFIG_WCE */
    if (!virtio_pci_feature_offered(dev, VIRTIO_BLK_F_CONFIG_WCE))
        return TEST_SKIP;

    if (dev->device_cfg_length <= VIRTIO_BLK_CFG_WCE_OFFSET)
        return TEST_SKIP;

    volatile uint8_t *wce = (volatile uint8_t *)dev->device_cfg +
                            VIRTIO_BLK_CFG_WCE_OFFSET;

    /* Read current value */
    uint8_t orig = *wce;

    /* Toggle to writethrough (0) */
    *wce = 0;
    __sync_synchronize();
    usleep(10000);

    /* Toggle to writeback (1) */
    *wce = 1;
    __sync_synchronize();
    usleep(10000);

    /* Restore */
    *wce = orig;
    __sync_synchronize();

    /* Verify device is still functional */
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    *st = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, st, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_REQUIRES(B0053, VIRTIO_PCI_DEVICE_BLK, test_blk_config_wce_toggle,
              "Toggle CONFIG_WCE writeback/writethrough mode",
              VIRTIO_SPEC_V1_2, "5.2.4",
              (1ULL << VIRTIO_BLK_F_CONFIG_WCE), 0);
