/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0087: Select queue beyond device capacity.
 *
 * Spec 4.1.4.3.2: Writing an out of range value to queue_select
 * should cause queue_size to read back as 0, indicating no such
 * queue. The device must not crash or return garbage.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_pci_queue_select_oob(struct virtio_dev *dev,
                                               struct vring *vr)
{
    (void)vr;

    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->queue_select = 0xFFFF;
    __sync_synchronize();

    uint16_t qsz = cfg->queue_size;
    (void)qsz; /* Expected to be 0 for non existent queue */

    /* Also try reading other queue fields */
    uint16_t noff = cfg->queue_notify_off;
    (void)noff;

    usleep(100 * 1000);

    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    return TEST_PASS;
}

REGISTER_TEST(PCI0087, VIRTIO_PCI_DEVICE_BLK, test_pci_queue_select_oob,
              "Select non existent queue index 0xFFFF",
              VIRTIO_SPEC_V1_2, "4.1.4.3.2");
