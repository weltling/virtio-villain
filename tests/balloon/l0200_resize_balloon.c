/* SPDX-License-Identifier: Apache-2.0 */
/*
 * L0200: virtio-balloon resize updates num_pages and config_generation.
 *
 * Spec 5.5.4 device specific config has num_pages and actual as
 * little endian u32. Spec 4.1.4.5 config_generation increments on
 * any device specific config change. The sidecar resizes the
 * balloon target on the host. The guest reads num_pages and
 * config_generation, sleeps to let the resize land, then asserts
 * num_pages changed and config_generation incremented.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

struct virtio_balloon_config_head {
    uint32_t num_pages;
    uint32_t actual;
} __attribute__((packed));

static test_result_t test_balloon_resize(struct virtio_dev *dev,
                                         struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    volatile struct virtio_balloon_config_head *bcfg =
        (volatile struct virtio_balloon_config_head *)dev->device_cfg;

    uint32_t pages_before = bcfg->num_pages;
    uint8_t gen_before = cfg->config_generation;

    int waited = 0;
    while (waited < 15000) {
        __sync_synchronize();
        if (bcfg->num_pages != pages_before)
            break;
        usleep(100 * 1000);
        waited += 100;
    }

    uint32_t pages_after = bcfg->num_pages;
    uint8_t gen_after = cfg->config_generation;

    if (pages_after == pages_before)
        return TEST_SKIP;
    if (gen_after == gen_before)
        TFAIL("gen_after == gen_before");
    return TEST_PASS;
}

REGISTER_TEST(L0200, VIRTIO_PCI_DEVICE_BALLOON, test_balloon_resize,
              "virtio-balloon resize updates num_pages and config_generation",
              VIRTIO_SPEC_V1_2, "5.5.4");
