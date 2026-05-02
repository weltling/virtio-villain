/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0009: pci_queue_select_oob
 *
 * Select a queue index that is >= num_queues reported by the device,
 * then attempt to configure it. A VMM that doesn't bounds-check
 * queue_select may write to an out-of-bounds queue array slot.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_pci_queue_select_oob(struct virtio_dev *dev,
                                               struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    (void)vr;

    uint16_t num_queues = cfg->num_queues;

    /* Select a queue index way beyond what the device has */
    uint16_t bad_queue = num_queues + 10;
    cfg->queue_select = bad_queue;
    __sync_synchronize();
    usleep(1000);

    /* Try to configure this non-existent queue */
    cfg->queue_size = 16;
    __sync_synchronize();

    struct vring vr2;
    vring_alloc(&vr2, 16);

    cfg->queue_desc = vr2.desc_phys;
    cfg->queue_avail = vr2.avail_phys;
    cfg->queue_used = vr2.used_phys;
    cfg->queue_msix_vector = 0xffff;
    cfg->queue_enable = 1;
    __sync_synchronize();
    usleep(10000);

    /* Try to kick the bogus queue */
    virtio_pci_kick(dev, bad_queue);
    usleep(200000);

    /* Also try maximum possible queue index */
    cfg->queue_select = 0xFFFF;
    __sync_synchronize();
    cfg->queue_size = 16;
    cfg->queue_enable = 1;
    __sync_synchronize();
    virtio_pci_kick(dev, 0xFFFF);
    usleep(100000);

    /* Verify device is alive - switch back to queue 0 */
    cfg->queue_select = 0;
    __sync_synchronize();
    uint16_t q0_size = cfg->queue_size;
    if (q0_size == 0)
        TFAIL("q0_size == 0");

    return TEST_PASS;
}

REGISTER_TEST(PCI0009, VIRTIO_PCI_DEVICE_BLK, test_pci_queue_select_oob,
              "Select queue index >= num_queues then configure it",
              VIRTIO_SPEC_V1_2, "4.1.4.3.1");
