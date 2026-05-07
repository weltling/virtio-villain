/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0052: reset by clearing each status bit one at a time
 *
 * Spec 2.1.2 step 1 says a reset is performed by writing 0 to the
 * status register. A driver that instead clears bits one at a
 * time eventually reaches 0 and the device must accept that as a
 * valid reset. This catches devices that only act on a single
 * write of 0, ignoring the intermediate values.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_piecewise_reset(struct virtio_dev *dev,
                                          struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    /* Bring status to ACKNOWLEDGE | DRIVER */
    virtio_pci_reset(dev);
    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    cfg->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();

    /* Clear DRIVER first then ACKNOWLEDGE */
    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    usleep(5000);
    virtio_pci_reset(dev);

    return TEST_PASS;
}

REGISTER_TEST(S0052, VIRTIO_PCI_DEVICE_BLK, test_piecewise_reset,
              "clear status bits one at a time then reach zero",
              VIRTIO_SPEC_V1_2, "2.1.2");
