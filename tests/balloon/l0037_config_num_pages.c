/* SPDX-License-Identifier: Apache-2.0 */
/*
 * L0037: read balloon config num_pages and actual fields.
 *
 * Spec 5.5.4: The device config contains num_pages (target balloon
 * size in 4K pages) and actual (current balloon size reported by
 * driver). Read both and verify num_pages is accessible. The actual
 * field starts at 0 before the driver inflates anything.
 */
#include "lib/virtio_test.h"

static test_result_t test_balloon_config_read(struct virtio_dev *dev)
{
    if (!dev->device_cfg || dev->device_cfg_length < 8)
        return TEST_SKIP;

    volatile struct virtio_balloon_config_head *bcfg =
        (volatile struct virtio_balloon_config_head *)dev->device_cfg;

    uint32_t num_pages = bcfg->num_pages;
    uint32_t actual = bcfg->actual;

    /* num_pages is the target set by the host; can be 0 initially */
    (void)num_pages;

    /* actual should be 0 since the driver has not inflated */
    if (actual != 0)
        TFAIL("actual %u, expected 0 before any inflate", actual);

    return TEST_PASS;
}

REGISTER_TEST_DEV(L0037, VIRTIO_PCI_DEVICE_BALLOON, test_balloon_config_read,
                  "Read balloon num_pages and actual config fields",
                  VIRTIO_SPEC_V1_2, "5.5.4");
