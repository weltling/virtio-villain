/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0026: IOMMU request body split across multiple descriptors.
 *
 * Submit an ATTACH whose body is split into two read-only
 * descriptors of arbitrary boundary (1 byte then the rest),
 * followed by the writable tail. The device must reassemble
 * the request correctly.
 *
 * Spec 2.7.5.3 (descriptor chains), 5.13.6.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_iommu_split_input(struct virtio_dev *dev,
                                            struct vring *vr)
{
    struct virtio_iommu_req_attach *req = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));

    req->head.type = VIRTIO_IOMMU_T_ATTACH;
    req->domain    = 0;
    req->endpoint  = 0;
    req->tail.status = 0xFF;

    uint64_t req_phys = vv_virt_to_phys(req);
    size_t   in_len   = (size_t)((uint8_t *)&req->tail - (uint8_t *)req);
    uint64_t tail_phys = req_phys + in_len;

    /* First chunk: 1 byte. Second chunk: rest of the body. */
    vring_raw_set_desc(vr, 0, req_phys, 1,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, req_phys + 1, in_len - 1,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, tail_phys, sizeof(req->tail),
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(I0026, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_split_input,
              "Request body split across descriptors",
              VIRTIO_SPEC_V1_2, "5.13.6");
