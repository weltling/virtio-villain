/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0105: IOMMU attach, map RW, verify both return S_OK.
 *
 * Spec 5.13.6: Submit ATTACH then MAP with READ|WRITE flags.
 * Verify both return status 0 (S_OK). Tests the positive path
 * for the full MAP permission set.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_iommu_map_rw(struct virtio_dev *dev,
                                       struct vring *vr)
{
    struct virtio_iommu_req_attach *a = vv_alloc_pages(1);
    struct virtio_iommu_req_map *m = vv_alloc_pages(1);

    memset(a, 0, sizeof(*a));
    memset(m, 0, sizeof(*m));

    a->head.type = VIRTIO_IOMMU_T_ATTACH;
    a->domain = 20;
    a->endpoint = 0;
    a->tail.status = 0xFF;

    void *backing = vv_alloc_pages(1);
    m->head.type = VIRTIO_IOMMU_T_MAP;
    m->domain = 20;
    m->virt_start = 0xC0000;
    m->virt_end = 0xC0FFF;
    m->phys_start = vv_virt_to_phys(backing);
    m->flags = VIRTIO_IOMMU_MAP_F_READ | VIRTIO_IOMMU_MAP_F_WRITE;
    m->tail.status = 0xFF;

    /* Submit ATTACH */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(a),
                       sizeof(*a) - sizeof(a->tail),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(&a->tail),
                       sizeof(a->tail), VRING_DESC_F_WRITE, 0);

    /* Submit MAP */
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(m),
                       sizeof(*m) - sizeof(m->tail),
                       VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, vv_virt_to_phys(&m->tail),
                       sizeof(m->tail), VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 2);
    vring_raw_set_avail_idx(vr, 2);

    test_result_t r = vv_kick_and_wait_n(dev, vr, 0, 2, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;

    if (a->tail.status != 0) TFAIL("attach status %u", a->tail.status);
    if (m->tail.status != 0) TFAIL("map status %u", m->tail.status);

    return TEST_PASS;
}

REGISTER_TEST(I0105, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_map_rw,
              "Attach then map RW both return S_OK",
              VIRTIO_SPEC_V1_2, "5.13.6");
