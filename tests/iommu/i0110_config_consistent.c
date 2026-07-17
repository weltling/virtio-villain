/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0110: IOMMU device config fields are internally consistent.
 *
 * Spec 5.13.4: page_size_mask is always present and the device MUST
 * set at least one bit in it (the least significant set bit defines
 * the mapping granularity). input_range, domain_range and bypass are
 * only valid when their feature bits are negotiated. Validate the
 * full le64 page_size_mask and, where the feature is offered, that
 * range starts do not exceed range ends and bypass is 0 or 1.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <unistd.h>

static test_result_t test_iommu_config_consistent(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    (void)vr;

    if (dev->device_cfg == NULL ||
        dev->device_cfg_length < sizeof(struct virtio_iommu_config))
        return TEST_SKIP;

    volatile struct virtio_iommu_config *cfg =
        (volatile struct virtio_iommu_config *)dev->device_cfg;

    uint64_t psm = cfg->page_size_mask;
    if (psm == 0)
        TFAIL("page_size_mask has no bits set");

    if (virtio_pci_feature_offered(dev, VIRTIO_IOMMU_F_INPUT_RANGE)) {
        uint64_t start = cfg->input_range.start;
        uint64_t end = cfg->input_range.end;
        if (start > end)
            TFAIL("input_range start 0x%llx > end 0x%llx",
                  (unsigned long long)start, (unsigned long long)end);
    }

    if (virtio_pci_feature_offered(dev, VIRTIO_IOMMU_F_DOMAIN_RANGE)) {
        uint32_t start = cfg->domain_range.start;
        uint32_t end = cfg->domain_range.end;
        if (start > end)
            TFAIL("domain_range start %u > end %u", start, end);
    }

    if (virtio_pci_feature_offered(dev, VIRTIO_IOMMU_F_BYPASS_CONFIG)) {
        uint8_t bypass = cfg->bypass;
        if (bypass > 1)
            TFAIL("bypass %u is neither 0 nor 1", bypass);
    }

    return TEST_PASS;
}

REGISTER_TEST(I0110, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_config_consistent,
              "IOMMU config fields are internally consistent",
              VIRTIO_SPEC_V1_2, "5.13.4");
