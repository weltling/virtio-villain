/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0107: IOMMU unmap returns S_OK.
 *
 * Spec 5.13.6.7: UNMAP removes a previously mapped range.
 * Submit ATTACH, MAP, UNMAP and verify UNMAP returns status 0.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_iommu_unmap_ok(struct virtio_dev *dev,
                                         struct vring *vr)
{
    struct virtio_iommu_req_attach *a = vv_alloc_pages(1);
    struct virtio_iommu_req_map *m = vv_alloc_pages(1);
    struct virtio_iommu_req_unmap *u = vv_alloc_pages(1);

    memset(a, 0, sizeof(*a));
    memset(m, 0, sizeof(*m));
    memset(u, 0, sizeof(*u));

    a->head.type = VIRTIO_IOMMU_T_ATTACH;
    a->domain = 40; a->endpoint = 0; a->tail.status = 0xFF;

    void *backing = vv_alloc_pages(1);
    m->head.type = VIRTIO_IOMMU_T_MAP;
    m->domain = 40; m->virt_start = 0xD0000; m->virt_end = 0xD0FFF;
    m->phys_start = vv_virt_to_phys(backing);
    m->flags = VIRTIO_IOMMU_MAP_F_READ | VIRTIO_IOMMU_MAP_F_WRITE;
    m->tail.status = 0xFF;

    u->head.type = VIRTIO_IOMMU_T_UNMAP;
    u->domain = 40; u->virt_start = 0xD0000; u->virt_end = 0xD0FFF;
    u->tail.status = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(a),
                       sizeof(*a) - sizeof(a->tail), VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(&a->tail),
                       sizeof(a->tail), VRING_DESC_F_WRITE, 0);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(m),
                       sizeof(*m) - sizeof(m->tail), VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, vv_virt_to_phys(&m->tail),
                       sizeof(m->tail), VRING_DESC_F_WRITE, 0);
    vring_raw_set_desc(vr, 4, vv_virt_to_phys(u),
                       sizeof(*u) - sizeof(u->tail), VRING_DESC_F_NEXT, 5);
    vring_raw_set_desc(vr, 5, vv_virt_to_phys(&u->tail),
                       sizeof(u->tail), VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 2);
    vring_raw_set_avail(vr, 2, 4);
    vring_raw_set_avail_idx(vr, 3);

    test_result_t r = vv_kick_and_wait_n(dev, vr, 0, 3, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;

    if (a->tail.status != 0) TFAIL("attach status %u", a->tail.status);
    if (m->tail.status != 0) TFAIL("map status %u", m->tail.status);
    if (u->tail.status != 0) TFAIL("unmap status %u", u->tail.status);

    return TEST_PASS;
}

REGISTER_TEST(I0107, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_unmap_ok,
              "Attach, map, unmap all return S_OK",
              VIRTIO_SPEC_V1_2, "5.13.6.7");
