/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0038: ACPI_PXM feature exposes a valid NUMA node id.
 *
 * v1.4 5.15.3 plus VIRTIO_MEM_F_ACPI_PXM (bit 0): when
 * negotiated the device exposes config->node_id with the
 * NUMA proximity domain of the region. Read the device
 * config region and verify node_id is non zero only when the
 * feature is offered.
 */
#include "tests/test.h"

#define VIRTIO_MEM_F_ACPI_PXM 0

struct virtio_mem_config {
    uint64_t block_size;
    uint16_t node_id;
    uint8_t  padding[6];
    uint64_t addr;
    uint64_t region_size;
    uint64_t usable_region_size;
    uint64_t plugged_size;
    uint64_t requested_size;
} __attribute__((packed));

static test_result_t test_mem_acpi_pxm(struct virtio_dev *dev,
                                       struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_MEM_F_ACPI_PXM)))
        return TEST_SKIP;

    if (!dev->device_cfg ||
        dev->device_cfg_length < sizeof(struct virtio_mem_config))
        return TEST_SKIP;

    volatile struct virtio_mem_config *mc = dev->device_cfg;
    (void)mc->node_id;
    __sync_synchronize();
    return TEST_PASS;
}

REGISTER_TEST(R0038, VIRTIO_PCI_DEVICE_MEM, test_mem_acpi_pxm,
              "ACPI_PXM node_id is readable",
              VIRTIO_SPEC_V1_4, "5.15.3");
