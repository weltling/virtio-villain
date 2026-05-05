/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0040: IOMMU device config layout sanity.
 *
 * Read the device specific configuration area and verify that
 * the layout matches struct virtio_iommu_config from spec
 * 5.13.4: at least page_size_mask must be non-zero. The test
 * only reads, never writes, so it never wedges the device.
 *
 * Spec 5.13.4.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <string.h>
#include <unistd.h>

struct virtio_iommu_config {
    uint32_t page_size_mask;
    struct {
        uint64_t start;
        uint64_t end;
    } input_range;
    struct {
        uint32_t start;
        uint32_t end;
    } domain_range;
    uint32_t probe_size;
    uint8_t  bypass;
    uint8_t  reserved[3];
} __attribute__((packed));

static test_result_t test_iommu_config_layout(struct virtio_dev *dev,
                                              struct vring *vr)
{
    (void)vr;

    if (dev->device_cfg == NULL)
        return TEST_SKIP;

    volatile struct virtio_iommu_config *cfg =
        (volatile struct virtio_iommu_config *)dev->device_cfg;

    uint32_t psm = cfg->page_size_mask;
    if (psm == 0)
        TFAIL("psm == 0");

    uint32_t probe = cfg->probe_size;
    (void)probe;

    return TEST_PASS;
}

REGISTER_TEST(I0040, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_config_layout,
              "Read device config and check page_size_mask",
              VIRTIO_SPEC_V1_2, "5.13.4");
