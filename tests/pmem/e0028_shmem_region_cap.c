/* SPDX-License-Identifier: Apache-2.0 */
/*
 * E0028: pmem shared memory region capability is reachable.
 *
 * v1.4 5.19.5: pmem exposes the persistent memory region via a
 * Shared Memory capability. Older implementations expose the same range
 * through the legacy device config, so validate whichever form is present.
 */
#include "tests/test.h"
#include "lib/virtio_spec.h"

static test_result_t test_pmem_shmem(struct virtio_dev *dev, struct vring *vr)
{
    (void)vr;
    if (dev->shared_memory_length > 0) {
        if (dev->shared_memory_id != 0)
            TFAIL("pmem shared memory region id is not 0");
        if (dev->shared_memory_bar > 5)
            TFAIL("pmem shared memory BAR is invalid");
        return TEST_PASS;
    }

    if (!dev->device_cfg ||
        dev->device_cfg_length < sizeof(struct pmem_config))
        return TEST_SKIP;

    volatile struct pmem_config *pc = dev->device_cfg;
    if (pc->size == 0)
        TFAIL("pmem reports zero region size");
    return TEST_PASS;
}

REGISTER_TEST(E0028, VIRTIO_PCI_DEVICE_PMEM, test_pmem_shmem,
              "pmem shared memory region size is non zero",
              VIRTIO_SPEC_V1_4, "5.19.5");
