/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0087: IOMMU PROBE with a 2 GB writable output descriptor.
 *
 * Spec 5.13.6.5: The driver supplies a writable buffer sized to
 * receive properties plus the tail. Submit a PROBE whose
 * writable descriptor declares a 2 GB length while the backing
 * buffer is only one page. A device that trusts the descriptor
 * length when memcpy'ing the response can write far past the
 * backing page and corrupt unrelated memory. The device must
 * cap its write at the actual properties size.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <string.h>

static test_result_t test_iommu_probe_huge_out(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_iommu_req_probe *req = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));
    req->head.type   = VIRTIO_IOMMU_T_PROBE;
    req->endpoint    = 0;
    req->tail.status = 0xFF;

    uint64_t req_phys = vv_virt_to_phys(req);
    size_t   in_len   = (size_t)((uint8_t *)&req->properties -
                                 (uint8_t *)req);
    uint64_t out_phys;
    vv_alloc_page_high(&out_phys);

    vring_raw_set_desc(vr, 0, req_phys, in_len,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, out_phys, 0x80000000u,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(I0087, VIRTIO_PCI_DEVICE_IOMMU,
              test_iommu_probe_huge_out,
              "Probe with 2 GB writable descriptor",
              VIRTIO_SPEC_V1_2, "5.13.6.5");
