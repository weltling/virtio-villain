/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0061: config_generation increments on config write.
 *
 * Spec 4.1.4.3.1: the device increments config_generation when
 * device-config changes atomically across multiple words. Read
 * generation twice; with no config update both reads should be
 * equal.
 */
#include "lib/virtio_test.h"

static test_result_t test_pci_config_gen(struct virtio_dev *dev)
{
    uint8_t g1 = dev->common->config_generation;
    usleep(2000);
    uint8_t g2 = dev->common->config_generation;
    if (g1 != g2)
        TFAIL("g1 != g2");
    return TEST_PASS;
}

REGISTER_TEST_DEV(PCI0061, 0, test_pci_config_gen,
                  "config_generation stable when idle",
                  VIRTIO_SPEC_V1_2, "4.1.4.3.1");
