/* SPDX-License-Identifier: Apache-2.0 */
#include "lib/msix_helpers.h"

static test_result_t check(struct msix_test *ctx)
{
    MSIX_CHECK(msix_activate(ctx, 0, 0));
    uint64_t before[2];
    MSIX_CHECK(msix_counts(ctx, before));
    MSIX_CHECK(msix_read(ctx, 0));
    MSIX_CHECK(msix_pending(ctx, 1));
    MSIX_CHECK(msix_expect(ctx, before, 0));
    MSIX_CHECK(msix_vector_mask(ctx, 0, 0));
    MSIX_CHECK(msix_expect(ctx, before, 1));
    MSIX_CHECK(msix_pending(ctx, 0));
    MSIX_CHECK(msix_counts(ctx, before));
    MSIX_CHECK(msix_read(ctx, 0));
    MSIX_CHECK(msix_expect(ctx, before, 1));
    return msix_pending(ctx, 0);
}

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    (void)vr;
    return msix_run(dev, 1, check);
}

REGISTER_TEST_REQUIRES(PCI0127, VIRTIO_PCI_DEVICE_BLK, test,
                       "pending MSI-X interrupt is delivered after vector unmask",
                       VIRTIO_SPEC_V1_2, "4.1.5.1.2",
                       1ULL << VIRTIO_F_VERSION_1, 1);
