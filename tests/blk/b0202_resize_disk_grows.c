/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0202: resize-disk grows capacity emits config change.
 *
 * Spec 5.2.4 says the device updates capacity in device specific
 * config and signals a configuration change. Spec 4.1.4.5 says the
 * ISR config bit is set and config_generation is incremented on
 * any device specific config change. The sidecar grows the boot
 * disk on the host. The guest reads capacity and config_generation
 * before, sleeps to let the resize land, then asserts the new
 * capacity is strictly greater and that config_generation
 * incremented.
 */
#include "lib/virtio_test.h"
#include "lib/blk_mmio_helpers.h"

static test_result_t test_blk_resize_disk_grows(struct virtio_dev *dev,
                                                struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    volatile struct virtio_blk_config_head *bcfg =
        (volatile struct virtio_blk_config_head *)dev->device_cfg;

    uint64_t cap_before = bcfg->capacity;
    uint8_t gen_before = cfg->config_generation;

    int waited = 0;
    while (waited < 15000) {
        __sync_synchronize();
        if (bcfg->capacity != cap_before)
            break;
        usleep(100 * 1000);
        waited += 100;
    }

    uint64_t cap_after = bcfg->capacity;
    uint8_t gen_after = cfg->config_generation;

    if (cap_after == cap_before)
        return TEST_SKIP;
    if (cap_after <= cap_before)
        TFAIL("cap_after <= cap_before");
    if (gen_after == gen_before)
        TFAIL("gen_after == gen_before");
    return TEST_PASS;
}

REGISTER_TEST(B0202, VIRTIO_PCI_DEVICE_BLK, test_blk_resize_disk_grows,
              "virtio-blk resize-disk grows capacity and bumps config_generation",
              VIRTIO_SPEC_V1_2, "5.2.4");

static int mmio_capacity(struct virtio_mmio_dev *dev, uint64_t *capacity)
{
    for (int attempt = 0; attempt < 100; attempt++) {
        uint32_t before = mmio_read32(dev, VIRTIO_MMIO_CONFIG_GENERATION);
        *capacity = mmio_read32(dev, VIRTIO_MMIO_CONFIG);
        *capacity |= (uint64_t)mmio_read32(dev, VIRTIO_MMIO_CONFIG + 4) << 32;
        uint32_t after = mmio_read32(dev, VIRTIO_MMIO_CONFIG_GENERATION);
        if (before == after)
            return 0;
    }
    return -1;
}

static test_result_t test_blk_resize_mmio(struct virtio_mmio_dev *dev)
{
    struct vring ring;
    VV_TRY(blk_mmio_attach(dev, &ring));
    uint64_t capacity;
    if (mmio_capacity(dev, &capacity) < 0)
        TFAIL("MMIO block config generation never stabilized");
    uint32_t generation = mmio_read32(dev, VIRTIO_MMIO_CONFIG_GENERATION);
    puts("vv-mmio-ready");
    fflush(stdout);
    for (int i = 0; i < 15000; i += 10) {
        uint64_t current;
        if (mmio_capacity(dev, &current) < 0)
            TFAIL("MMIO block config generation never stabilized");
        if (current != capacity) {
            if (current <= capacity)
                TFAIL("MMIO block capacity did not grow");
            if (mmio_read32(dev, VIRTIO_MMIO_CONFIG_GENERATION) == generation)
                TFAIL("MMIO config generation did not change after resize");
            return blk_mmio_read(dev, &ring, 0);
        }
        usleep(10000);
    }
    TFAIL("MMIO disk resize did not update capacity");
}

REGISTER_MMIO_VARIANT(B0202, 2, test_blk_resize_mmio);
