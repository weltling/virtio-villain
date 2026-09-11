/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0044: validate virtio-mem config alignment invariants.
 *
 * Spec 5.15.4: block_size must be a power of two, addr must be aligned
 * to block_size, and usable_region_size and plugged_size must be
 * multiples of block_size with plugged_size no greater than
 * usable_region_size. R0041 covers the region_size constraints; this
 * covers the alignment and plugged/usable relationship.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <unistd.h>

static test_result_t test_mem_config_alignment(struct virtio_dev *dev,
                                               struct vring *vr)
{
    (void)vr;

    if (!dev->device_cfg ||
        dev->device_cfg_length < sizeof(struct virtio_mem_config))
        return TEST_SKIP;

    volatile struct virtio_mem_config *mcfg =
        (volatile struct virtio_mem_config *)dev->device_cfg;

    uint64_t block_size = mcfg->block_size;
    uint64_t addr = mcfg->addr;
    uint64_t usable = mcfg->usable_region_size;
    uint64_t plugged = mcfg->plugged_size;

    if (block_size == 0)
        TFAIL("block_size is 0");
    if (block_size & (block_size - 1))
        TFAIL("block_size %llu is not a power of two",
              (unsigned long long)block_size);
    if (addr % block_size != 0)
        TFAIL("addr %llu not aligned to block_size %llu",
              (unsigned long long)addr, (unsigned long long)block_size);
    if (usable % block_size != 0)
        TFAIL("usable_region_size %llu not a multiple of block_size %llu",
              (unsigned long long)usable, (unsigned long long)block_size);
    if (plugged % block_size != 0)
        TFAIL("plugged_size %llu not a multiple of block_size %llu",
              (unsigned long long)plugged, (unsigned long long)block_size);
    if (plugged > usable)
        TFAIL("plugged_size %llu > usable_region_size %llu",
              (unsigned long long)plugged, (unsigned long long)usable);

    return TEST_PASS;
}

REGISTER_TEST(R0044, VIRTIO_PCI_DEVICE_MEM, test_mem_config_alignment,
              "Validate mem config block_size and plugged alignment",
              VIRTIO_SPEC_V1_2, "5.15.4");
