/* SPDX-License-Identifier: Apache-2.0 */
#include "lib/msix_helpers.h"

static test_result_t check(struct msix_test *ctx)
{
    MSIX_CHECK(msix_prepare_live(ctx));
    puts("vv-msix-pause-ready");
    fflush(stdout);
    unsigned matched;
    MSIX_CHECK(msix_wait_host(ctx, "vv-msix-pause-done", NULL, &matched));
    return msix_finish_live(ctx);
}

static test_result_t test(struct virtio_dev *dev)
{
    return msix_run(dev, 1, check);
}

REGISTER_TEST_DEV_REQUIRES(PCI0129, VIRTIO_PCI_DEVICE_BLK, test,
                           "live MSI-X routing survives host pause and resume",
                           VIRTIO_SPEC_V1_2, "4.1.5.1.2.1",
                           1ULL << VIRTIO_F_VERSION_1, 1);
