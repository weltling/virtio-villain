/* SPDX-License-Identifier: Apache-2.0 */
/*
 * E0032: read pmem config start address and capacity.
 *
 * Spec 5.16.4: When VIRTIO_PMEM_F_SHMEM_REGION is offered the
 * device config contains start (offset 0) and capacity (offset 8).
 * Without the feature, the device still has a start and size pair.
 * Read both and verify capacity is non zero.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <unistd.h>

static test_result_t test_pmem_config(struct virtio_dev *dev,
                                      struct vring *vr)
{
    (void)vr;

    if (!dev->device_cfg || dev->device_cfg_length < 16)
        return TEST_SKIP;

    volatile uint64_t *cfg64 = (volatile uint64_t *)dev->device_cfg;
    uint64_t start = cfg64[0];
    uint64_t capacity = cfg64[1];

    (void)start;

    if (capacity == 0)
        TFAIL("pmem capacity is 0");

    return TEST_PASS;
}

REGISTER_TEST(E0032, VIRTIO_PCI_DEVICE_PMEM, test_pmem_config,
              "Read pmem config start and capacity",
              VIRTIO_SPEC_V1_2, "5.16.4");
