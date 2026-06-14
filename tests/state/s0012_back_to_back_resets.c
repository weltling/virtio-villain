/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0012: back_to_back_resets
 *
 * Write device_status=0 twice rapidly without waiting for the readback
 * to confirm the first reset completed. The spec says the driver MUST
 * wait for a read of device_status to return 0 before reinitializing
 * (4.1.4.3.2).
 *
 * This tests whether the VMM's reset handler is re-entrant. Some VMMs
 * spawn an async reset task; a second reset arriving before the first
 * completes may corrupt internal state or double-free resources.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_back_to_back_resets(struct virtio_dev *dev,
                                              struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    (void)vr;

    /* Reset #1 - don't wait for completion */
    cfg->device_status = 0;

    /* Reset #2 - immediately, no barrier, no readback */
    cfg->device_status = 0;

    /* Reset #3 - one more for good measure */
    cfg->device_status = 0;
    __sync_synchronize();

    /* Now rapidly start re-initialization without waiting */
    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();

    /* Check if device accepted it (may still be resetting) */
    usleep(1000);
    if (!(cfg->device_status & VIRTIO_STATUS_ACKNOWLEDGE)) {
        /* Device still resetting - wait properly this time */
        usleep(50000);
        if (cfg->device_status != 0) {
            /* Device is confused */
            return TEST_PASS;
        }
        cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
        __sync_synchronize();
    }

    cfg->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();

    cfg->driver_feature_select = 0;
    cfg->driver_feature = 0;
    cfg->driver_feature_select = 1;
    cfg->driver_feature = 0;
    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();

    if (!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK))
        return TEST_SKIP;

    struct vring vr2;
    vring_alloc(&vr2, 16);
    vring_attach(dev, &vr2, 0);
    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    /* Verify device is operational */
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    vring_raw_set_desc(&vr2, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&vr2, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(&vr2, 2, vv_virt_to_phys(status), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&vr2, 0, 0);
    vring_raw_set_avail_idx(&vr2, 1);

    return vv_kick_and_wait(dev, &vr2, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(S0012, VIRTIO_PCI_DEVICE_BLK, test_back_to_back_resets,
              "Multiple device resets without waiting for completion",
              VIRTIO_SPEC_V1_2, "4.1.4.3.2");
