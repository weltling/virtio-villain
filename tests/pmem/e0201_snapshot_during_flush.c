/* SPDX-License-Identifier: Apache-2.0 */
/*
 * E0201: virtio-pmem device survives a snapshot of the running VM.
 *
 * Spec 2.6 ties device state to a model that tolerates save and
 * restore. The sidecar pauses the VM, takes a snapshot, and
 * resumes. The guest watches device_status across the window and
 * asserts the device never raises FAILED or NEEDS_RESET and that
 * DRIVER_OK is still set on exit.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_pmem_survives_snapshot(struct virtio_dev *dev,
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

REGISTER_TEST(E0201, VIRTIO_PCI_DEVICE_PMEM,
              test_pmem_survives_snapshot,
              "virtio-pmem survives a host triggered snapshot",
              VIRTIO_SPEC_V1_2, "2.6");
