/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0123: HOST_ECN requires HOST_TSO4 or HOST_TSO6.
 *
 * v1.4 5.1.3 plus VIRTIO_NET_F_HOST_ECN (bit 13): HOST_ECN
 * is only meaningful in combination with at least one
 * HOST_TSO* feature. If HOST_ECN is offered without either
 * TSO it is a spec violation.
 */
#include "tests/test.h"

#define VIRTIO_NET_F_HOST_TSO4 11
#define VIRTIO_NET_F_HOST_TSO6 12
#define VIRTIO_NET_F_HOST_ECN  13

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t lo = cfg->device_feature;
    if (!(lo & (1U << VIRTIO_NET_F_HOST_ECN)))
        return TEST_SKIP;
    if (!(lo & ((1U << VIRTIO_NET_F_HOST_TSO4) |
                (1U << VIRTIO_NET_F_HOST_TSO6))))
        TFAIL("HOST_ECN offered without HOST_TSO4 or HOST_TSO6");
    return TEST_PASS;
}

REGISTER_TEST(N0123, VIRTIO_PCI_DEVICE_NET, test,
              "HOST_ECN paired with a HOST_TSO feature",
              VIRTIO_SPEC_V1_4, "5.1.3");
