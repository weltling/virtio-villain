/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0201: virtio-vsock device survives a pause and resume cycle.
 *
 * Spec 2.6 requires the device to tolerate pause and resume of the
 * VM as a whole. The sidecar pauses, sleeps briefly, then resumes.
 * The guest watches device_status and asserts vsock does not raise
 * FAILED or NEEDS_RESET and that DRIVER_OK remains set.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_vsock_survives_pause(struct virtio_dev *dev,
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

REGISTER_TEST(V0201, VIRTIO_PCI_DEVICE_VSOCK,
              test_vsock_survives_pause,
              "virtio-vsock survives a pause and resume cycle",
              VIRTIO_SPEC_V1_2, "2.6");
