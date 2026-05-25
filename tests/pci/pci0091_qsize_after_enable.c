/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0091: Write queue_size = 0xFFFF after queue_enable.
 *
 * Spec 4.1.4.3 and 2.7: queue_size must be a power of two and
 * the driver should program it before queue_enable. Select
 * queue 0, enable it, then write 0xFFFF to queue_size. A
 * device that resizes the ring on the fly without re reading
 * queue_enable can attempt to allocate or walk a 65535 entry
 * ring against the existing descriptor area pointer. The
 * device must ignore the late write or reject it; it must
 * not crash.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_pci_qsize_after_enable(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->queue_select = 0;
    __sync_synchronize();

    uint16_t before = cfg->queue_size;
    cfg->queue_size = 0xFFFF;
    __sync_synchronize();

    usleep(50 * 1000);

    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");

    uint16_t after = cfg->queue_size;
    if (after != before && (after & (after - 1)) != 0)
        TFAIL("queue_size accepted a non power of two value");

    return TEST_PASS;
}

REGISTER_TEST(PCI0091, VIRTIO_PCI_DEVICE_BLK,
              test_pci_qsize_after_enable,
              "Write queue_size 0xFFFF after queue_enable",
              VIRTIO_SPEC_V1_2, "4.1.4.3");
