/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0068: suspend_resume
 *
 * Suspend the device, then resume by setting DRIVER_OK. Spec 3.2:
 * the device sets SUSPEND to 0 once it resumes running.
 */
#include "lib/virtio_test.h"

static test_result_t test_suspend_resume(struct virtio_dev *dev,
                                         struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* Check if SUSPEND is offered */
    cfg->device_feature_select = VIRTIO_F_SUSPEND / 32;
    __sync_synchronize();
    uint32_t offered = cfg->device_feature;
    if (!(offered & (1U << (VIRTIO_F_SUSPEND % 32))))
        return TEST_SKIP;

    /* Suspend */
    uint8_t status = cfg->device_status;
    cfg->device_status = status | VIRTIO_STATUS_SUSPEND;
    __sync_synchronize();
    usleep(200000);

    uint8_t suspended = cfg->device_status;
    if (!(suspended & VIRTIO_STATUS_SUSPEND))
        TFAIL("!(suspended & VIRTIO_STATUS_SUSPEND)");

    /* Resume by setting DRIVER_OK */
    cfg->device_status = suspended | VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();
    usleep(200000);

    /* Verify: SUSPEND=0, DRIVER_OK=1 */
    uint8_t resumed = cfg->device_status;
    if (resumed & VIRTIO_STATUS_SUSPEND)
        TFAIL("resumed & VIRTIO_STATUS_SUSPEND");
    if (!(resumed & VIRTIO_STATUS_DRIVER_OK))
        TFAIL("!(resumed & VIRTIO_STATUS_DRIVER_OK)");

    /* I/O should work again */
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    *st = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, st, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_REQUIRES(S0068, VIRTIO_PCI_DEVICE_BLK, test_suspend_resume,
              "Suspend then resume device and verify I/O works",
              VIRTIO_SPEC_V1_3, "3.2",
              (1ULL << VIRTIO_F_SUSPEND), 0);
