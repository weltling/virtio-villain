/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0031: After device signals DEVICE_NEEDS_RESET, attempt a full reset
 * sequence and verify the device is usable again.
 *
 * Spec 2.1.2: When NEEDS_RESET is signaled, the driver should reset
 * and reinitialize. This test provokes an error (invalid desc), waits
 * to see if NEEDS_RESET appears, then resets and verifies recovery.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_needs_reset_recovery(struct virtio_dev *dev,
                                               struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /*
     * Try to provoke NEEDS_RESET by submitting a descriptor with
     * addr=0 and len=0 (completely invalid).
     */
    vring_raw_set_desc(vr, 0, 0, 0, VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, 0, 0, VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    __sync_synchronize();
    virtio_pci_kick(dev, vr->queue);

    /* Wait briefly to see if NEEDS_RESET appears */
    int elapsed = 0;
    int saw_needs_reset = 0;
    while (elapsed < VV_TIMEOUT_MS * 1000) {
        usleep(10000);
        __sync_synchronize();
        uint8_t status = cfg->device_status;
        if (status & VIRTIO_STATUS_NEEDS_RESET) {
            saw_needs_reset = 1;
            break;
        }
        if (status == 0) {
            /* Device self-reset */
            saw_needs_reset = 1;
            break;
        }
        elapsed += 10000;
    }

    /*
     * Whether or not NEEDS_RESET was signaled (some devices may just
     * reject silently), attempt a full reset and recovery.
     */
    (void)saw_needs_reset;

    /* Reset */
    cfg->device_status = 0;
    __sync_synchronize();

    int tries = 200;
    while (tries-- > 0 && cfg->device_status != 0)
        usleep(1000);

    if (cfg->device_status != 0)
        TWEDGED("cfg->device_status != 0");

    /* Full re-init */
    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER;
    __sync_synchronize();

    cfg->driver_feature_select = 0;
    cfg->driver_feature = 0;
    cfg->driver_feature_select = 1;
    cfg->driver_feature = 0;
    __sync_synchronize();

    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();
    usleep(5000);

    if (!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK))
        TREJECT("!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK)");

    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    /* Verify device works: submit a valid read */
    struct vring new_vr;
    vring_alloc(&new_vr, 16);
    vring_attach(dev, &new_vr, 0);

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *st = 0xFF;

    vring_raw_set_desc(&new_vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&new_vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(&new_vr, 2, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(&new_vr, 0, 0);
    vring_raw_set_avail_idx(&new_vr, 1);

    return vv_kick_and_wait(dev, &new_vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(S0031, VIRTIO_PCI_DEVICE_BLK, test_needs_reset_recovery,
              "Recovery after DEVICE_NEEDS_RESET via full reset sequence",
              VIRTIO_SPEC_V1_2, "2.1.2");
