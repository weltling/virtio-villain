/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0012: IOMMU request with status descriptor not writable.
 *
 * The tail (status) descriptor of every virtio-iommu request
 * MUST carry VRING_DESC_F_WRITE so the device can write the
 * status byte back. Submit a request whose tail descriptor lacks
 * the WRITE flag. The device must reject the request and stay
 * alive.
 *
 * Spec 5.13.6, 2.7.5.3.1.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_iommu_status_not_writable(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    struct virtio_iommu_req_attach *req = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));

    req->head.type = VIRTIO_IOMMU_T_ATTACH;
    req->tail.status = 0xFF;

    uint64_t req_phys = vv_virt_to_phys(req);
    size_t   in_len   = (size_t)((uint8_t *)&req->tail - (uint8_t *)req);
    uint64_t tail_phys = req_phys + in_len;

    /* Status descriptor is read-only by the device: no WRITE flag. */
    vring_raw_set_desc(vr, 0, req_phys, in_len,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, tail_phys, sizeof(req->tail),
                       0, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(I0012, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_status_not_writable,
              "Status descriptor not marked writable",
              VIRTIO_SPEC_V1_2, "5.13.6");
