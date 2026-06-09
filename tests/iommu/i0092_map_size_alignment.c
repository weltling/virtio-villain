/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0092: MAP virt_start and virt_end with sub page alignment.
 *
 * v1.4 5.13.6.3: virt_start and virt_end MUST be page aligned.
 * Submit a MAP with virt_start = 0x100 and virt_end = 0xFFE
 * (sub page boundaries); the device must reject.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/virtio_iommu.h"

#include <string.h>

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    struct virtio_iommu_req_map *m = vv_alloc_pages(1);
    memset(m, 0, sizeof(*m));
    m->head.type = VIRTIO_IOMMU_T_MAP;
    m->domain = 0;
    m->virt_start = 0x100;
    m->virt_end = 0xFFE;
    m->phys_start = 0;
    m->flags = VIRTIO_IOMMU_MAP_F_READ;
    m->tail.status = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(m),
                       (uint32_t)((uint8_t *)&m->tail - (uint8_t *)m),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(&m->tail),
                       sizeof(m->tail), VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(I0092, VIRTIO_PCI_DEVICE_IOMMU, test,
              "MAP with sub page boundaries",
              VIRTIO_SPEC_V1_4, "5.13.6.3");
