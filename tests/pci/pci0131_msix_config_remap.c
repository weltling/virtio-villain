/* SPDX-License-Identifier: Apache-2.0 */
#include "lib/msix_helpers.h"

static test_result_t capacity(struct msix_test *ctx, uint64_t *value,
                               uint8_t *generation)
{
    for (unsigned attempt = 0; attempt < 100; attempt++) {
        uint8_t before = ctx->dev->common->config_generation;
        __sync_synchronize();
        *value = vv_mmio_read32(ctx->dev->device_cfg);
        *value |= (uint64_t)vv_mmio_read32(
            (volatile uint8_t *)ctx->dev->device_cfg + 4) << 32;
        __sync_synchronize();
        *generation = ctx->dev->common->config_generation;
        if (before == *generation)
            return TEST_PASS;
    }
    TFAIL("block configuration generation never stabilized");
}

static test_result_t resize_event(struct msix_test *ctx, unsigned phase)
{
    uint64_t before, current;
    uint8_t generation, changed;
    MSIX_CHECK(capacity(ctx, &before, &generation));
    printf("vv-msix-config-ready-%u\n", phase);
    fflush(stdout);
    int scale = vv_wait_scale > 0 ? vv_wait_scale : 1;
    for (int i = 0; i < 2000 * scale; i++) {
        MSIX_CHECK(capacity(ctx, &current, &changed));
        if (current != before) {
            if (current != before + 16 * 1024 * 1024 / 512)
                TFAIL("host resize did not grow capacity by 16 MiB");
            if (changed == generation)
                TFAIL("configuration generation did not change after resize");
            return msix_read(ctx, 0);
        }
        usleep(10000);
    }
    TFAIL("host resize did not update block capacity");
}

static test_result_t check(struct msix_test *ctx)
{
    if (!ctx->dev->device_cfg || ctx->dev->device_cfg_length < 8)
        TFAIL("block capacity configuration is unavailable");
    MSIX_CHECK(msix_config_vector(ctx, 0));
    MSIX_CHECK(msix_activate(ctx, VIRTIO_MSI_NO_VECTOR, 0));
    uint64_t before[2];
    for (unsigned phase = 0; phase < 3; phase++) {
        uint16_t vector = phase < 2 ? phase : VIRTIO_MSI_NO_VECTOR;
        MSIX_CHECK(msix_config_vector(ctx, vector));
        MSIX_CHECK(msix_counts(ctx, before));
        MSIX_CHECK(resize_event(ctx, phase));
        MSIX_CHECK(msix_pending(ctx, phase < 2 ? 1u << phase : 0));
        MSIX_CHECK(msix_expect(ctx, before, 0));
        if (phase < 2) {
            MSIX_CHECK(msix_vector_mask(ctx, phase, 0));
            MSIX_CHECK(msix_expect(ctx, before, 1u << phase));
            MSIX_CHECK(msix_pending(ctx, 0));
        }
    }
    return TEST_PASS;
}

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    (void)vr;
    return msix_run(dev, 1, check);
}

REGISTER_TEST_REQUIRES(PCI0131, VIRTIO_PCI_DEVICE_BLK, test,
                       "configuration events follow live MSI-X remapping and unmapping",
                       VIRTIO_SPEC_V1_2, "4.1.5.1.2.1",
                       1ULL << VIRTIO_F_VERSION_1, 1);
