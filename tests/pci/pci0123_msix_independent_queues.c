/* SPDX-License-Identifier: Apache-2.0 */
#include "lib/msix_helpers.h"

static test_result_t check(struct msix_test *ctx)
{
    MSIX_CHECK(msix_activate(ctx, 0, 1));
    MSIX_CHECK(msix_queue_vector(ctx, 0, VIRTIO_MSI_NO_VECTOR));
    ctx->dev->common->queue_select = 1;
    __sync_synchronize();
    if (ctx->dev->common->queue_msix_vector != 1)
        TFAIL("unmapping queue 0 changed queue 1 mapping");
    uint64_t before[2];
    MSIX_CHECK(msix_counts(ctx, before));
    MSIX_CHECK(msix_read(ctx, 0));
    MSIX_CHECK(msix_expect(ctx, before, 0));
    MSIX_CHECK(msix_pending(ctx, 0));
    MSIX_CHECK(msix_read(ctx, 1));
    MSIX_CHECK(msix_pending(ctx, 2));
    MSIX_CHECK(msix_vector_mask(ctx, 1, 0));
    MSIX_CHECK(msix_expect(ctx, before, 2));
    MSIX_CHECK(msix_pending(ctx, 0));
    MSIX_CHECK(msix_queue_vector(ctx, 0, 0));
    MSIX_CHECK(msix_read(ctx, 0));
    MSIX_CHECK(msix_pending(ctx, 1));
    MSIX_CHECK(msix_counts(ctx, before));
    MSIX_CHECK(msix_read(ctx, 1));
    MSIX_CHECK(msix_expect(ctx, before, 2));
    MSIX_CHECK(msix_pending(ctx, 1));
    MSIX_CHECK(msix_counts(ctx, before));
    MSIX_CHECK(msix_vector_mask(ctx, 0, 0));
    MSIX_CHECK(msix_expect(ctx, before, 1));
    return msix_pending(ctx, 0);
}

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    (void)vr;
    return msix_run(dev, 2, check);
}

REGISTER_TEST_REQUIRES(PCI0123, VIRTIO_PCI_DEVICE_BLK, test,
                       "independent MSI-X mappings for two live queues",
                       VIRTIO_SPEC_V1_2, "4.1.5.1.2.1",
                       (1ULL << VIRTIO_F_VERSION_1) | (1ULL << VIRTIO_BLK_F_MQ), 2);
