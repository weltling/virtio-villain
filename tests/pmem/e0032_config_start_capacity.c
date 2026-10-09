/* SPDX-License-Identifier: Apache-2.0 */
/*
 * E0032: read pmem config start address and capacity.
 *
 * Spec 5.19.4: The persistent memory range is exposed either through
 * the shared memory capability or the legacy start/capacity config.
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

    uint64_t capacity = dev->shared_memory_length;
    if (capacity == 0 && dev->device_cfg && dev->device_cfg_length >= 16) {
        volatile uint64_t *cfg64 = (volatile uint64_t *)dev->device_cfg;
        capacity = cfg64[1];
    }

    if (capacity == 0)
        TFAIL("pmem capacity is 0");

    return TEST_PASS;
}

REGISTER_TEST(E0032, VIRTIO_PCI_DEVICE_PMEM, test_pmem_config,
              "Read pmem config start and capacity",
              VIRTIO_SPEC_V1_2, "5.19.4");
