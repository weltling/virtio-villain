/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0076: admin_queue_index_read
 *
 * Read the admin_queue_index and admin_queue_num fields from the
 * PCI common cfg. Spec v1.3 4.1.4.3.1: these fields identify the
 * admin virtqueue(s) when the device supports admin commands.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

static test_result_t test_pci_admin_queue_index(struct virtio_dev *dev,
                                                struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    /* Read admin queue fields from common cfg extended region.
     * admin_queue_index is at offset 0x44, admin_queue_num at 0x46
     * (beyond the standard struct). If the common cfg region is
     * too small, skip. */
    if (dev->common_length < 0x48)
        return TEST_SKIP;

    volatile uint8_t *raw = (volatile uint8_t *)cfg;
    uint16_t num_queues = cfg->num_queues;
    uint16_t admin_idx = *(volatile uint16_t *)(raw + 0x44);
    uint16_t admin_num = *(volatile uint16_t *)(raw + 0x46);

    /* Basic sanity: admin_queue_index + admin_queue_num must not
     * exceed a reasonable bound */
    if (admin_num > 0 && admin_idx >= num_queues + admin_num)
        TFAIL("admin_num > 0 && admin_idx >= num_queues + admin_num");

    return TEST_PASS;
}

REGISTER_TEST(PCI0076, VIRTIO_PCI_DEVICE_BLK, test_pci_admin_queue_index,
              "Read admin_queue_index and admin_queue_num from common cfg",
              VIRTIO_SPEC_V1_3, "4.1.4.3.1");
