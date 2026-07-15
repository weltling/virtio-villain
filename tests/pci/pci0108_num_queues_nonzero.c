/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0108: common config num_queues is non zero.
 *
 * Spec 4.1.4.3: The num_queues field reports the total number of
 * virtqueues. It must be at least 1 for any functional device.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <unistd.h>

static test_result_t test_pci_num_queues(struct virtio_dev *dev,
                                         struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    uint16_t nq = cfg->num_queues;
    if (nq == 0)
        TFAIL("num_queues is 0");

    return TEST_PASS;
}

REGISTER_TEST(PCI0108, VIRTIO_PCI_DEVICE_BLK, test_pci_num_queues,
              "Common config num_queues is non zero",
              VIRTIO_SPEC_V1_2, "4.1.4.3");
