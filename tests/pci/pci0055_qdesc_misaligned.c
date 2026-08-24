/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0055: queue_desc misaligned address.
 *
 * Spec 2.7.4 requires descriptor table 16-byte alignment. Write
 * a misaligned phys_addr to queue_desc and check the VMM does
 * not blindly use it.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_pci_qdesc_misaligned(struct virtio_dev *dev,
                                               struct vring *vr)
{
    (void)vr;
    uint64_t saved = virtio_load64(&dev->common->queue_desc);
    dev->common->queue_select = 0;
    __sync_synchronize();
    virtio_store64(&dev->common->queue_desc, saved + 1);
    __sync_synchronize();
    usleep(5000);
    virtio_store64(&dev->common->queue_desc, saved);
    return TEST_PASS;
}

REGISTER_TEST(PCI0055, 0, test_pci_qdesc_misaligned,
              "queue_desc misaligned",
              VIRTIO_SPEC_V1_2, "2.7.4");
