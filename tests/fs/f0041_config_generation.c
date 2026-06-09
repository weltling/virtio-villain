/* SPDX-License-Identifier: Apache-2.0 */
/*
 * F0041: config_generation reads are stable.
 *
 * v1.4 4.1.4.3.1: config_generation increments only when
 * config fields change. Read it twice with no intervening
 * operation; the values must be equal.
 */
#include "tests/test.h"

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    uint8_t a = cfg->config_generation;
    __sync_synchronize();
    uint8_t b = cfg->config_generation;
    if (a != b)
        TFAIL("config_generation drifted: %u then %u", a, b);
    return TEST_PASS;
}

REGISTER_TEST(F0041, VIRTIO_PCI_DEVICE_FS, test,
              "config_generation stable across two reads",
              VIRTIO_SPEC_V1_4, "4.1.4.3.1");
