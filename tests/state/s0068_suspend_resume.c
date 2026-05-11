/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0068: suspend_resume
 *
 * Suspend the device, then resume by setting DRIVER_OK. Spec 3.2:
 * the device sets SUSPEND to 0 once it resumes running.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

#define VIRTIO_F_SUSPEND 43
#define VIRTIO_STATUS_SUSPEND 16
#define VIRTIO_BLK_T_IN 0

struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} __attribute__((packed));

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
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *st = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(S0068, VIRTIO_PCI_DEVICE_BLK, test_suspend_resume,
              "Suspend then resume device and verify I/O works",
              VIRTIO_SPEC_V1_3, "3.2");
