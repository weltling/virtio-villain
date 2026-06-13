/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0135: Write to read-only config fields.
 *
 * Attempt to write to device-read-only configuration fields such as
 * capacity. Device must not change its capacity.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_blk_write_ro_config(struct virtio_dev *dev,
                                              struct vring *vr)
{
    (void)vr;
    volatile uint8_t *cfg_space = (volatile uint8_t *)dev->device_cfg;

    /* Read original capacity (first 8 bytes of block config) */
    uint64_t orig_cap;
    memcpy(&orig_cap, (void *)cfg_space, sizeof(orig_cap));

    /* Attempt to write a bogus capacity */
    uint64_t bogus = 0xDEADBEEFDEADBEEF;
    volatile uint64_t *cap_ptr = (volatile uint64_t *)cfg_space;
    *cap_ptr = bogus;
    __sync_synchronize();

    /* Read back */
    uint64_t readback;
    memcpy(&readback, (void *)cfg_space, sizeof(readback));

    if (readback == bogus)
        TFAIL("readback == bogus");

    return TEST_PASS;
}

REGISTER_TEST(B0135, VIRTIO_PCI_DEVICE_BLK, test_blk_write_ro_config,
              "Write to read-only config fields",
              VIRTIO_SPEC_V1_2, "5.2.4");
