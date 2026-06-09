/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0129: HOST_USO bit must come with CSUM offload.
 *
 * v1.4 5.1.3 plus VIRTIO_NET_F_HOST_USO (bit 56) for UDP GSO
 * offload. Without CSUM the device cannot generate valid UDP
 * checksums for the segmented packets, so HOST_USO offered
 * without CSUM is a spec violation.
 */
#include "tests/test.h"

#define VIRTIO_NET_F_CSUM      0
#define VIRTIO_NET_F_HOST_USO 56

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    cfg->device_feature_select = 1;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << (VIRTIO_NET_F_HOST_USO - 32))))
        return TEST_SKIP;
    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_NET_F_CSUM)))
        TFAIL("HOST_USO offered without CSUM");
    return TEST_PASS;
}

REGISTER_TEST(N0129, VIRTIO_PCI_DEVICE_NET, test,
              "HOST_USO paired with CSUM",
              VIRTIO_SPEC_V1_4, "5.1.3");
