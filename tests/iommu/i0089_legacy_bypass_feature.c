/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0089: BYPASS_CONFIG feature bit handling.
 *
 * v1.4 5.13.3 plus VIRTIO_IOMMU_F_BYPASS_CONFIG (bit 6):
 * when negotiated the device exposes a bypass flag in the
 * config that the driver can flip at runtime. Verify a
 * config region is present whenever the bit is offered.
 */
#include "tests/test.h"
#include "lib/virtio_iommu.h"

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_IOMMU_F_BYPASS_CONFIG)))
        return TEST_SKIP;
    if (!dev->device_cfg || dev->device_cfg_length < 16)
        TFAIL("BYPASS_CONFIG offered but device cfg too small");
    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(I0089, VIRTIO_PCI_DEVICE_IOMMU, test,
              "BYPASS_CONFIG offered implies usable config",
              VIRTIO_SPEC_V1_4, "5.13.3",
              (1ULL << VIRTIO_IOMMU_F_BYPASS_CONFIG), 0);
