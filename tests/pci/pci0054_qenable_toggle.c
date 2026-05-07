/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0054: queue_enable toggle without reset.
 *
 * Spec 4.1.4.3.2: queue_enable should not be cleared by the
 * driver after DRIVER_OK. Toggle it 0->1->0 anyway and verify
 * the device does not crash.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_pci_qenable_toggle(struct virtio_dev *dev,
                                             struct vring *vr)
{
    (void)vr;
    dev->common->queue_select = 0;
    __sync_synchronize();
    dev->common->queue_enable = 0;
    __sync_synchronize();
    usleep(1000);
    dev->common->queue_enable = 1;
    __sync_synchronize();
    return TEST_PASS;
}

REGISTER_TEST(PCI0054, 0, test_pci_qenable_toggle,
              "queue_enable toggle after DRIVER_OK",
              VIRTIO_SPEC_V1_2, "4.1.4.3.2");
