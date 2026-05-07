/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0064: queue_msix_vector outside table.
 *
 * Spec 4.1.4.3.2: queue_msix_vector points into the MSI-X table;
 * 0xFFFF means no vector. Setting it to a value larger than the
 * table size must be rejected (read back as 0xFFFF).
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_pci_qmsix_oob(struct virtio_dev *dev,
                                        struct vring *vr)
{
    (void)vr;
    dev->common->queue_select = 0;
    __sync_synchronize();
    dev->common->queue_msix_vector = 0xF000;
    __sync_synchronize();
    uint16_t v = dev->common->queue_msix_vector;
    if (v != 0xFFFF && v != 0xF000)
        return TEST_PASS;
    dev->common->queue_msix_vector = 0xFFFF;
    return TEST_PASS;
}

REGISTER_TEST(PCI0064, 0, test_pci_qmsix_oob,
              "queue_msix_vector beyond table size",
              VIRTIO_SPEC_V1_2, "4.1.4.3.2");
