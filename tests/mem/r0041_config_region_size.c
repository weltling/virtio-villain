/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0041: read virtio-mem config region_size and validate.
 *
 * Spec 5.15.4: The device config exposes region_size at offset 24
 * (after block_size[8]+node_id[2]+padding[6]+addr[8]). It must be
 * non zero, a multiple of block_size, and at least as large as
 * usable_region_size.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <unistd.h>

static test_result_t test_mem_config_region_size(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    (void)vr;

    if (!dev->device_cfg || dev->device_cfg_length < sizeof(struct virtio_mem_config))
        return TEST_SKIP;

    volatile struct virtio_mem_config *mcfg =
        (volatile struct virtio_mem_config *)dev->device_cfg;

    uint64_t block_size = mcfg->block_size;
    uint64_t region_size = mcfg->region_size;
    uint64_t usable = mcfg->usable_region_size;

    if (block_size == 0)
        TFAIL("block_size is 0");
    if (region_size == 0)
        TFAIL("region_size is 0");
    if (region_size % block_size != 0)
        TFAIL("region_size %llu not a multiple of block_size %llu",
              (unsigned long long)region_size,
              (unsigned long long)block_size);
    if (usable > region_size)
        TFAIL("usable_region_size %llu > region_size %llu",
              (unsigned long long)usable,
              (unsigned long long)region_size);

    return TEST_PASS;
}

REGISTER_TEST(R0041, VIRTIO_PCI_DEVICE_MEM, test_mem_config_region_size,
              "Read region_size config and validate constraints",
              VIRTIO_SPEC_V1_2, "5.15.4");
