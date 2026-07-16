/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0112: config_generation is readable.
 *
 * Spec 4.1.4.3.1: config_generation provides a generation counter
 * for device config reads. Read it and verify it does not cause
 * a device fault. The value itself is opaque.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <unistd.h>

static test_result_t test_pci_config_gen(struct virtio_dev *dev,
                                         struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* Just read it twice; the value should be stable with no config change */
    uint8_t g1 = cfg->config_generation;
    __sync_synchronize();
    uint8_t g2 = cfg->config_generation;

    /* They should be equal since no config change happened */
    if (g1 != g2)
        TFAIL("config_generation changed (%u -> %u) without stimulus",
              g1, g2);

    return TEST_PASS;
}

REGISTER_TEST(PCI0112, VIRTIO_PCI_DEVICE_BLK, test_pci_config_gen,
              "config_generation is readable and stable",
              VIRTIO_SPEC_V1_2, "4.1.4.3.1");
