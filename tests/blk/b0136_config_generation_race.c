/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0136: Config generation counter race.
 *
 * Read config space fields while monitoring the generation counter.
 * If generation changes between start and end of a multi-field read,
 * the read must be retried.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>

static test_result_t test_blk_config_generation_race(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /*
     * Read capacity using generation check protocol:
     * 1. Read config_generation
     * 2. Read config fields
     * 3. Read config_generation again
     * 4. If different, retry
     */
    volatile uint8_t *cfg_space = (volatile uint8_t *)dev->device_cfg;
    uint64_t cap;
    uint8_t gen_before, gen_after;
    int attempts = 0;

    do {
        gen_before = cfg->config_generation;
        __sync_synchronize();
        memcpy(&cap, (void *)cfg_space, sizeof(cap));
        __sync_synchronize();
        gen_after = cfg->config_generation;
        attempts++;
        if (attempts > 100)
            TREJECT("attempts > 100");
    } while (gen_before != gen_after);

    /* Capacity must be non-zero for a valid block device */
    if (cap == 0)
        TFAIL("cap == 0");

    return TEST_PASS;
}

REGISTER_TEST(B0136, VIRTIO_PCI_DEVICE_BLK, test_blk_config_generation_race,
              "Config generation counter consistency check",
              VIRTIO_SPEC_V1_2, "5.2.4");
