/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0070: probe before attach lifecycle
 *
 * Spec 5.13.5 lists PROBE as the first operation a driver performs
 * to discover endpoint capabilities before it ATTACHes the
 * endpoint to a domain. Submit PROBE then ATTACH then DETACH in a
 * single batch and verify all three complete. A VMM that requires
 * the endpoint to already be attached before allowing PROBE will
 * reject the first request.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_iommu_probe_before_attach(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    struct virtio_iommu_req_probe  *p = vv_alloc_pages(1);
    struct virtio_iommu_req_attach *a = vv_alloc_pages(1);
    struct virtio_iommu_req_detach *d = vv_alloc_pages(1);
    memset(p, 0, sizeof(*p));
    memset(a, 0, sizeof(*a));
    memset(d, 0, sizeof(*d));

    p->head.type = VIRTIO_IOMMU_T_PROBE;
    p->endpoint = 0;
    p->tail.status = 0xFF;

    a->head.type = VIRTIO_IOMMU_T_ATTACH;
    a->domain = 7;
    a->endpoint = 0;
    a->tail.status = 0xFF;

    d->head.type = VIRTIO_IOMMU_T_DETACH;
    d->domain = 7;
    d->endpoint = 0;
    d->tail.status = 0xFF;

    uint64_t p_phys = vv_virt_to_phys(p);
    uint64_t a_phys = vv_virt_to_phys(a);
    uint64_t d_phys = vv_virt_to_phys(d);
    size_t p_in = (size_t)((uint8_t *)&p->tail - (uint8_t *)p);
    size_t a_in = (size_t)((uint8_t *)&a->tail - (uint8_t *)a);
    size_t d_in = (size_t)((uint8_t *)&d->tail - (uint8_t *)d);

    vring_raw_set_desc(vr, 0, p_phys, p_in, VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, p_phys + p_in, sizeof(p->tail),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_desc(vr, 2, a_phys, a_in, VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, a_phys + a_in, sizeof(a->tail),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_desc(vr, 4, d_phys, d_in, VRING_DESC_F_NEXT, 5);
    vring_raw_set_desc(vr, 5, d_phys + d_in, sizeof(d->tail),
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 2);
    vring_raw_set_avail(vr, 2, 4);
    vring_raw_set_avail_idx(vr, 3);

    return vv_kick_and_wait_n(dev, vr, 0, 3, VV_TIMEOUT_MS);
}

REGISTER_TEST(I0070, VIRTIO_PCI_DEVICE_IOMMU,
              test_iommu_probe_before_attach,
              "Probe then attach then detach in one batch",
              VIRTIO_SPEC_V1_2, "5.13.5");
