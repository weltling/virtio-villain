/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0093: virt addr range wrap around.
 *
 * v1.4 5.13.6.3: virt_end must be greater than or equal to
 * virt_start. Set virt_start near 0xFFFF_FFFF_FFFF_F000 and
 * virt_end below it (range wraps). The device must reject.
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
    m->virt_start = 0xFFFFFFFFFFFFF000ULL;
    m->virt_end   = 0x0000000000000FFFULL;
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

REGISTER_TEST(I0093, VIRTIO_PCI_DEVICE_IOMMU, test,
              "MAP with virt range wrapping the address space",
              VIRTIO_SPEC_V1_4, "5.13.6.3");
