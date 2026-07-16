/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0108: IOMMU attach verifies S_OK status explicitly.
 *
 * Submit a single ATTACH request and verify the tail status byte
 * is exactly 0 (S_OK). Prior attach tests often batch with MAP
 * or DETACH; this isolates the attach response.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_iommu_attach_verify(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_iommu_req_attach *a = vv_alloc_pages(1);
    memset(a, 0, sizeof(*a));

    a->head.type = VIRTIO_IOMMU_T_ATTACH;
    a->domain = 50;
    a->endpoint = 0;
    a->tail.status = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(a),
                       sizeof(*a) - sizeof(a->tail),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(&a->tail),
                       sizeof(a->tail), VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;

    if (a->tail.status != 0)
        TFAIL("attach status %u, expected 0 (S_OK)", a->tail.status);

    return TEST_PASS;
}

REGISTER_TEST(I0108, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_attach_verify,
              "Single ATTACH returns S_OK",
              VIRTIO_SPEC_V1_2, "5.13.6.2");
