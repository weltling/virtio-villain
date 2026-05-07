/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0063: write config_generation read-only.
 *
 * Spec 4.1.4.3.1: config_generation is read-only for the driver.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_pci_cfg_gen_ro(struct virtio_dev *dev,
                                         struct vring *vr)
{
    (void)vr;
    uint8_t before = dev->common->config_generation;
    dev->common->config_generation = (uint8_t)(before + 1);
    __sync_synchronize();
    uint8_t after = dev->common->config_generation;
    if (after != before)
        TFAIL("after != before");
    return TEST_PASS;
}

REGISTER_TEST(PCI0063, 0, test_pci_cfg_gen_ro,
              "config_generation is read-only",
              VIRTIO_SPEC_V1_2, "4.1.4.3.1");
