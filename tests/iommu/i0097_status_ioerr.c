/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0097: response carries S_IOERR for an impossible MAP.
 *
 * v1.4 5.13.6: the device fills request.tail.status with one
 * of the S_* codes. Submit a MAP into an unattached domain;
 * the device must respond with a non OK status.
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
    m->domain = 0xDEADBEEFu;
    m->virt_start = 0;
    m->virt_end = 0xFFF;
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
    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;
    if (m->tail.status == VIRTIO_IOMMU_S_OK)
        TFAIL("MAP into unattached domain returned OK");
    return TEST_PASS;
}

REGISTER_TEST(I0097, VIRTIO_PCI_DEVICE_IOMMU, test,
              "Response status non OK for impossible MAP",
              VIRTIO_SPEC_V1_4, "5.13.6");
