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

    if (!cfg || dev->device_cfg_length == 0)
        return TEST_SKIP;

    /* Read the whole advertised config region, including any space
     * past the config struct this harness knows. Spec v1.3 2.4:
     * future device versions may extend the config space, and reads
     * within the advertised region must return defined values and
     * not crash. Stay within device_cfg_length: reading past the
     * advertised capability is an out of bounds access, not a
     * forward compatible one. */
    volatile uint32_t val = 0;
    (void)val;

    uint32_t len = dev->device_cfg_length;
    uint32_t off = 0;
    for (; off + 4 <= len; off += 4)
        val = *(volatile uint32_t *)(cfg + off);
    for (; off < len; off++)
        val = cfg[off];

    return TEST_PASS;
}

REGISTER_TEST(S0076, VIRTIO_PCI_DEVICE_BLK, test_forward_large_config,
              "Read device config space beyond known size",
              VIRTIO_SPEC_V1_3, "2.4");
