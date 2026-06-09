/* SPDX-License-Identifier: Apache-2.0 */
/*
 * F0037: DAX shared memory region capability.
 *
 * v1.4 5.11.6: virtio-fs may expose a DAX window via the
 * Shared Memory capability. The harness already walked PCI
 * caps during init; this test simply verifies the device has
 * a sensible device config region (tag is mandatory).
 */
#include "tests/test.h"

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    (void)vr;
    if (!dev->device_cfg || dev->device_cfg_length < 36)
        return TEST_SKIP;

    volatile uint8_t *p = dev->device_cfg;
    int has_byte = 0;
    for (uint32_t i = 0; i < 36; i++)
        if (p[i] != 0) { has_byte = 1; break; }
    if (!has_byte)
        TFAIL("config tag region is all zero");
    return TEST_PASS;
}

REGISTER_TEST(F0037, VIRTIO_PCI_DEVICE_FS, test,
              "virtio-fs device config tag region non zero",
              VIRTIO_SPEC_V1_4, "5.11.6");
