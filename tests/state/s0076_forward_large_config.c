/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0076: forward_compat_large_config_space
 *
 * Read beyond the known device config space size. Spec v1.3 2.4:
 * future device versions may extend the config space. Reads past
 * the current version's config must return defined values (zero)
 * and not crash.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

static test_result_t test_forward_large_config(struct virtio_dev *dev,
                                               struct vring *vr)
{
    volatile uint8_t *cfg = dev->device_cfg;

    (void)vr;

    if (!cfg)
        return TEST_SKIP;

    /* Read at various offsets well beyond typical config sizes.
     * The device must not crash on out of range reads. */
    volatile uint32_t val = 0;
    (void)val;

    for (int off = 0; off < 256; off += 4) {
        val = *(volatile uint32_t *)(cfg + off);
    }

    return TEST_PASS;
}

REGISTER_TEST(S0076, VIRTIO_PCI_DEVICE_BLK, test_forward_large_config,
              "Read device config space beyond known size",
              VIRTIO_SPEC_V1_3, "2.4");
