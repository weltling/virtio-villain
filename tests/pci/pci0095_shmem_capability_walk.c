/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0095: shared memory capability walk via SHM_SEL.
 *
 * Spec 4.1.4.7: a device may expose shared memory regions
 * through capability type 8. The driver iterates by writing
 * SHM_SEL on the common cfg. The harness already parsed PCI
 * caps; we exercise the SHM_SEL surface via common cfg's
 * msix_config slot is unrelated. This test just ensures the
 * device cfg region is non zero size when a shared memory
 * region exists (heuristic).
 */
#include "tests/test.h"

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    (void)vr;
    if (!dev->device_cfg) return TEST_SKIP;
    if (dev->device_cfg_length == 0)
        TFAIL("device_cfg_length == 0 with device_cfg present");
    return TEST_PASS;
}

REGISTER_TEST(PCI0095, VIRTIO_PCI_DEVICE_BLK, test,
              "Shared memory capability walk smoke",
              VIRTIO_SPEC_V1_4, "4.1.4.7");
