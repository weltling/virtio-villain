/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0077: admin_queue_select
 *
 * Select the admin queue via queue_select and read its queue_size.
 * Spec v1.3 4.1.4.3.1: if admin_queue_num > 0, selecting
 * admin_queue_index should expose a valid queue_size.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

static test_result_t test_pci_admin_queue_select(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    if (dev->common_length < 0x48)
        return TEST_SKIP;

    volatile uint8_t *raw = (volatile uint8_t *)cfg;
    uint16_t admin_num = *(volatile uint16_t *)(raw + 0x46);
    if (admin_num == 0)
        return TEST_SKIP;

    uint16_t admin_idx = *(volatile uint16_t *)(raw + 0x44);

    cfg->queue_select = admin_idx;
    __sync_synchronize();

    uint16_t qs = cfg->queue_size;
    /* Admin queue must have a non zero size if it exists */
    if (qs == 0)
        TFAIL("qs == 0");

    return TEST_PASS;
}

REGISTER_TEST(PCI0077, VIRTIO_PCI_DEVICE_BLK, test_pci_admin_queue_select,
              "Select admin queue and verify non zero queue_size",
              VIRTIO_SPEC_V1_3, "4.1.4.3.1");
