/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0090: Write to the read only queue_notify_off field.
 *
 * Spec 4.1.4.3: queue_notify_off is a read only field that the
 * device populates for each selected queue. The driver MUST NOT
 * write to it. Perform a write to queue_notify_off after
 * selecting queue 0 and verify the original device assigned
 * value is preserved on readback and the device remains alive.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_pci_write_queue_notify_off(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->queue_select = 0;
    __sync_synchronize();

    uint16_t before = cfg->queue_notify_off;
    cfg->queue_notify_off = 0xBEEF;
    __sync_synchronize();
    uint16_t after = cfg->queue_notify_off;

    usleep(50 * 1000);

    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");

    if (after != before)
        TFAIL("queue_notify_off mutated by driver write");

    return TEST_PASS;
}

REGISTER_TEST(PCI0090, VIRTIO_PCI_DEVICE_BLK,
              test_pci_write_queue_notify_off,
              "Driver write to read only queue_notify_off",
              VIRTIO_SPEC_V1_2, "4.1.4.3");
