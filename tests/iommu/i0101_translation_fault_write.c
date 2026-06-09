/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0101: MAP with read only flag accepted.
 *
 * v1.4 5.13.6.3: MAP with flags = READ permits reads only.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/virtio_iommu.h"

#include <string.h>

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    struct virtio_iommu_req_attach *a = vv_alloc_pages(1);
    struct virtio_iommu_req_map    *m = vv_alloc_pages(1);
    memset(a, 0, sizeof(*a));
    memset(m, 0, sizeof(*m));

    a->head.type = VIRTIO_IOMMU_T_ATTACH;
    a->domain = 0xA101; a->endpoint = 0; a->tail.status = 0xFF;
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(a),
                       (uint32_t)((uint8_t *)&a->tail - (uint8_t *)a),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(&a->tail),
                       sizeof(a->tail), VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;

    m->head.type = VIRTIO_IOMMU_T_MAP;
    m->domain = 0xA101;
    m->virt_start = 0;
    m->virt_end = 0xFFF;
    m->phys_start = 0;
    m->flags = VIRTIO_IOMMU_MAP_F_READ;
    m->tail.status = 0xFF;
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(m),
                       (uint32_t)((uint8_t *)&m->tail - (uint8_t *)m),
                       VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, vv_virt_to_phys(&m->tail),
                       sizeof(m->tail), VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 1, 2);
    vring_raw_set_avail_idx(vr, 2);
    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(I0101, VIRTIO_PCI_DEVICE_IOMMU, test,
              "MAP with READ only flag accepted",
              VIRTIO_SPEC_V1_4, "5.13.6.3");
