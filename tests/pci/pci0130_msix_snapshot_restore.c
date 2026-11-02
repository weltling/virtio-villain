/* SPDX-License-Identifier: Apache-2.0 */
#include "lib/msix_helpers.h"

static test_result_t check(struct msix_test *ctx)
{
    MSIX_CHECK(msix_prepare_live(ctx));
    puts("vv-msix-snapshot-ready");
    fflush(stdout);
    unsigned matched;
    MSIX_CHECK(msix_wait_host(ctx, "vv-msix-saved", "vv-msix-restored", &matched));
    if (matched == 0) {
        MSIX_CHECK(msix_queue_vector(ctx, 0, 0));
        puts("vv-msix-snapshot-mutated");
        fflush(stdout);
        MSIX_CHECK(msix_wait_host(ctx, "vv-msix-restored", NULL, &matched));
        TFAIL("snapshot load did not restore guest execution state");
    }
    return msix_finish_live(ctx);
}

static test_result_t test(struct virtio_dev *dev)
{
    return msix_run(dev, 1, check);
}

REGISTER_TEST_DEV_REQUIRES(PCI0130, VIRTIO_PCI_DEVICE_BLK, test,
                           "snapshot restore reconstructs live MSI-X routing",
                           VIRTIO_SPEC_V1_2, "4.1.5.1.2.1",
                           1ULL << VIRTIO_F_VERSION_1, 1);
