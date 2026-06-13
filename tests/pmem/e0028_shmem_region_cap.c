/* SPDX-License-Identifier: Apache-2.0 */
/*
 * E0028: pmem shared memory region capability is reachable.
 *
 * v1.4 5.19.5: pmem exposes the persistent memory region via a
 * Shared Memory capability. The harness already walked PCI caps
 * during init, so we only confirm a device cfg region exists
 * (some implementations also surface the start/size there).
 */
#include "tests/test.h"
#include "lib/virtio_spec.h"

static test_result_t test_pmem_shmem(struct virtio_dev *dev, struct vring *vr)
{
    (void)vr;
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
