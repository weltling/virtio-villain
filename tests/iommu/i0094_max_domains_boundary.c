/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0094: ATTACH with domain just above domain_range max.
 *
 * v1.4 5.13.3 plus VIRTIO_IOMMU_F_DOMAIN_RANGE: the config
 * exposes domain_range.end; attaching with domain_id =
 * end + 1 must be rejected. Without DOMAIN_RANGE the test
 * skips.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/virtio_iommu.h"

#include <string.h>

struct iommu_config {
    uint32_t page_size_mask;
    struct { uint64_t start, end; } input_range;
    struct { uint32_t start, end; } domain_range;
} __attribute__((packed));

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_IOMMU_F_DOMAIN_RANGE)))
        return TEST_SKIP;

    if (!dev->device_cfg ||
        dev->device_cfg_length < sizeof(struct iommu_config))
        return TEST_SKIP;
    volatile struct iommu_config *c = dev->device_cfg;
    uint32_t bad = c->domain_range.end + 1;
    if (bad < c->domain_range.end) return TEST_SKIP;

    struct virtio_iommu_req_attach *a = vv_alloc_pages(1);
    memset(a, 0, sizeof(*a));
    a->head.type = VIRTIO_IOMMU_T_ATTACH;
    a->domain = bad;
    a->endpoint = 0;
    a->tail.status = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(a),
                       (uint32_t)((uint8_t *)&a->tail - (uint8_t *)a),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(&a->tail),
                       sizeof(a->tail), VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(I0094, VIRTIO_PCI_DEVICE_IOMMU, test,
              "ATTACH with domain_id above domain_range.end",
              VIRTIO_SPEC_V1_4, "5.13.3");
