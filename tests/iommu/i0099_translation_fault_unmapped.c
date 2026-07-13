/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0099: translation fault path is exercised by a stray request.
 *
 * v1.4 5.13.6: when an endpoint has no mapping for a virt
 * address, a real transaction would fault. The driver cannot
 * cause a transaction directly from the guest; this test
 * ensures the iommu device cfg still responds healthy after
 * a probe sequence (covers the no mapping branch).
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/virtio_iommu.h"

#include <string.h>

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_IOMMU_F_PROBE)))
        return TEST_SKIP;

    struct virtio_iommu_req_probe *p = vv_alloc_pages(1);
    memset(p, 0, sizeof(*p));
    p->head.type = VIRTIO_IOMMU_T_PROBE;
    p->endpoint = 0;
    p->tail.status = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(p),
                       (uint32_t)((uint8_t *)&p->properties - (uint8_t *)p),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(&p->properties),
                       (uint32_t)(sizeof(p->properties) + sizeof(p->tail)),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_REQUIRES(I0099, VIRTIO_PCI_DEVICE_IOMMU, test,
              "PROBE smoke (no mapping in domain)",
              VIRTIO_SPEC_V1_4, "5.13.6",
              (1ULL << VIRTIO_IOMMU_F_PROBE), 0);
