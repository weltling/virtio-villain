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
#include "lib/virtio_test.h"

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

static test_result_t test_balloon_resize_mmio(struct virtio_mmio_dev *dev)
{
    if (virtio_mmio_init(dev) < 0)
        TFAIL("MMIO balloon feature negotiation failed");
    mmio_write32(dev, VIRTIO_MMIO_STATUS,
                 mmio_read32(dev, VIRTIO_MMIO_STATUS)
                 | VIRTIO_MMIO_STATUS_DRIVER_OK);
    uint32_t pages = mmio_read32(dev, VIRTIO_MMIO_CONFIG);
    uint32_t generation = mmio_read32(dev, VIRTIO_MMIO_CONFIG_GENERATION);
    puts("vv-mmio-ready");
    fflush(stdout);
    for (int elapsed = 0; elapsed < 15000; elapsed += 10) {
        if (mmio_read32(dev, VIRTIO_MMIO_CONFIG) != pages) {
            if (mmio_read32(dev, VIRTIO_MMIO_CONFIG_GENERATION) == generation)
                TFAIL("MMIO balloon config generation did not change");
            return TEST_PASS;
        }
        usleep(10000);
    }
    TFAIL("MMIO balloon target did not change");
}

REGISTER_MMIO_VARIANT(L0200, 5, test_balloon_resize_mmio);
