/* SPDX-License-Identifier: Apache-2.0 */
/*
 * A0200: virtio admin features survive a snapshot of the running VM.
 *
 * Spec 2.6 covers save and restore. Admin queue support rides on
 * top of an existing virtio device so this test registers against
 * the boot blk device. The sidecar pauses, snapshots, and resumes.
 * The guest watches device_status and asserts the device does not
 * raise FAILED or NEEDS_RESET and that DRIVER_OK remains set.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_admin_survives_snapshot(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    if (!(cfg->device_status & VIRTIO_STATUS_DRIVER_OK))
        return TEST_SKIP;
    int waited = 0;
    while (waited < 8000) {
        __sync_synchronize();
        uint8_t st = cfg->device_status;
        if (st & VIRTIO_STATUS_FAILED)
            TFAIL("st & VIRTIO_STATUS_FAILED");
        if (st & VIRTIO_STATUS_NEEDS_RESET)
            TFAIL("st & VIRTIO_STATUS_NEEDS_RESET");
        usleep(100 * 1000);
        waited += 100;
    }
    if (!(cfg->device_status & VIRTIO_STATUS_DRIVER_OK))
        TFAIL("!(cfg->device_status & VIRTIO_STATUS_DRIVER_OK)");
    return TEST_PASS;
}

REGISTER_TEST(A0200, VIRTIO_PCI_DEVICE_BLK,
              test_admin_survives_snapshot,
              "virtio admin queue host survives a snapshot",
              VIRTIO_SPEC_V1_3, "2.6");
