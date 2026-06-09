/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0122: HOST_TSO6 feature offered without GUEST_TSO6.
 *
 * v1.4 5.1.3 plus VIRTIO_NET_F_HOST_TSO6 (bit 12): if the
 * device offers HOST_TSO6 it MUST also offer HOST_TSO4 and
 * relies on GUEST_TSO6 being offered for symmetrical behavior
 * with mergeable buffers. Verify HOST_TSO6, if advertised,
 * comes with HOST_TSO4.
 */
#include "tests/test.h"

#define VIRTIO_NET_F_GUEST_TSO4   7
#define VIRTIO_NET_F_GUEST_TSO6   8
#define VIRTIO_NET_F_HOST_TSO4   11
#define VIRTIO_NET_F_HOST_TSO6   12

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t lo = cfg->device_feature;
    if (!(lo & (1U << VIRTIO_NET_F_HOST_TSO6)))
        return TEST_SKIP;
    if (!(lo & (1U << VIRTIO_NET_F_HOST_TSO4)))
        TFAIL("HOST_TSO6 offered without HOST_TSO4");
    return TEST_PASS;
}

REGISTER_TEST(N0122, VIRTIO_PCI_DEVICE_NET, test,
              "HOST_TSO6 paired with HOST_TSO4",
              VIRTIO_SPEC_V1_4, "5.1.3");
