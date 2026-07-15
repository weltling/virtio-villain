/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0099: common config queue_size is non zero and power of two.
 *
 * Spec 4.1.4.3.2: queue_size indicates the maximum number of
 * elements in the queue. It must be non zero (queue exists) and
 * should be a power of two for split virtqueues.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <unistd.h>

static test_result_t test_pci_queue_size(struct virtio_dev *dev,
                                         struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->queue_select = 0;
    __sync_synchronize();
    uint16_t qsize = cfg->queue_size;

    if (qsize == 0)
        TFAIL("queue_size is 0 for queue 0");

    /* Must be power of two for split virtqueues */
    if ((qsize & (qsize - 1)) != 0)
        TFAIL("queue_size %u is not a power of two", qsize);

    return TEST_PASS;
}

REGISTER_TEST(PCI0099, VIRTIO_PCI_DEVICE_BLK, test_pci_queue_size,
              "Common config queue_size is non zero and power of two",
              VIRTIO_SPEC_V1_2, "4.1.4.3.2");
