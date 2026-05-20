/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0089: Double queue enable.
 *
 * Spec 4.1.4.3.2: Enable a queue that is already enabled. The
 * spec says the driver must not enable a queue already enabled.
 * The device must handle this protocol violation without
 * corrupting state or crashing.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_pci_double_queue_enable(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    (void)vr;

    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* Queue 0 is already enabled by the harness; enable it again */
    cfg->queue_select = 0;
    __sync_synchronize();
    cfg->queue_enable = 1;
    __sync_synchronize();

    usleep(100 * 1000);

    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    return TEST_PASS;
}

REGISTER_TEST(PCI0089, VIRTIO_PCI_DEVICE_BLK, test_pci_double_queue_enable,
              "Enable an already enabled queue",
              VIRTIO_SPEC_V1_2, "4.1.4.3.2");
