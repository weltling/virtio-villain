/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0074: iommu_attach_endpoint_to_two_domains
 *
 * Attach endpoint 0 to domain 1, then attach the same endpoint
 * to domain 2 without first issuing DETACH. Spec 5.13.6.1 forbids
 * binding an endpoint to two domains. The second attach must
 * fail and the first binding must remain usable.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_iommu_two_domains(struct virtio_dev *dev,
                                            struct vring *vr)
{
    struct virtio_iommu_req_attach *req1 = vv_alloc_pages(1);
    struct virtio_iommu_req_attach *req2 = vv_alloc_pages(1);
    memset(req1, 0, sizeof(*req1));
    memset(req2, 0, sizeof(*req2));

    req1->head.type = VIRTIO_IOMMU_T_ATTACH;
    req1->domain = 1;
    req1->endpoint = 0;
    req1->tail.status = 0xFF;

    req2->head.type = VIRTIO_IOMMU_T_ATTACH;
    req2->domain = 2;
    req2->endpoint = 0;
    req2->tail.status = 0xFF;

    uint64_t r1 = vv_virt_to_phys(req1);
    uint64_t r2 = vv_virt_to_phys(req2);
    size_t in_len = (size_t)((uint8_t *)&req1->tail - (uint8_t *)req1);

    vring_raw_set_desc(vr, 0, r1, in_len, VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, r1 + in_len, sizeof(req1->tail),
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_desc(vr, 2, r2, in_len, VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, r2 + in_len, sizeof(req2->tail),
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 2);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait_n(dev, vr, 0, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST(I0074, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_two_domains,
              "Attach endpoint to two domains without detach",
              VIRTIO_SPEC_V1_2, "5.13.6.1");
