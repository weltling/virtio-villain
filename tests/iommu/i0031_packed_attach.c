/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0031: IOMMU ATTACH on a packed virtqueue.
 *
 * Send a basic ATTACH using the packed ring layout. Verifies
 * the device handles virtio-iommu requests on a packed queue
 * the same way it handles them on split queues.
 *
 * Spec 2.8, 5.13.6.1.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring_packed.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_iommu_packed_attach(struct virtio_dev *dev,
                                              struct vring_packed *vr)
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

    vring_packed_set_desc(vr, 0, req_phys, in_len, 0,
                          VRING_PACKED_DESC_F_NEXT);
    vring_packed_advance(vr);
    vring_packed_set_desc(vr, 1, tail_phys, sizeof(req->tail), 1,
                          VRING_PACKED_DESC_F_WRITE);
    vring_packed_advance(vr);

    __sync_synchronize();
    virtio_pci_kick(dev, 0);
    usleep(500000);
    return TEST_PASS;
}

REGISTER_TEST_PACKED(I0031, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_packed_attach,
                     "Attach via packed virtqueue",
                     VIRTIO_SPEC_V1_2, "2.8");
