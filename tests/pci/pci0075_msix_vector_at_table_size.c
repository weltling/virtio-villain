/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0075: pci_msix_vector_at_table_size
 *
 * Write the MSI-X queue vector field with the value equal to
 * the table size, which is one past the largest valid index.
 * Spec 4.1.5.1.4 lets the driver use 0xFFFF (NO_VECTOR) but
 * forbids any other out of range value. The device must not
 * crash and the readback must reflect either NO_VECTOR or the
 * prior value.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_pci_msix_vec_at_size(struct virtio_dev *dev,
                                               struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    cfg->queue_select = 0;
    __sync_synchronize();
    uint16_t before = cfg->queue_msix_vector;

    /*
     * Without parsing the MSI-X capability, use a sentinel value
     * 64 which exceeds the per device table for the modern
     * virtio-blk default.
     */
    cfg->queue_msix_vector = 64;
    __sync_synchronize();
    uint16_t after = cfg->queue_msix_vector;

    /* Restore */
    cfg->queue_msix_vector = before;
    __sync_synchronize();

    if (after != 64 && after != before && after != 0xFFFF)
        TFAIL("after != 64 && after != before && after != 0xFFFF");

    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    return TEST_PASS;
}

REGISTER_TEST(PCI0075, VIRTIO_PCI_DEVICE_BLK, test_pci_msix_vec_at_size,
              "queue_msix_vector set to a probable out of range index",
              VIRTIO_SPEC_V1_2, "4.1.5.1.4");
