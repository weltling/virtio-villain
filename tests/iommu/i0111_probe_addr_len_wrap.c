/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0111: IOMMU probe response writable addr plus len wraps 2^64.
 *
 * Siblings I0075, I0079 and I0087 keep the response end address below
 * 2^64. Here the probe output descriptor base sits near the top of the
 * address space and the length makes addr plus len wrap to a low value,
 * so a device that computes the end with a plain addition gets a small
 * wrapped result and a naive bounds check passes. The device must not
 * access memory outside the guest mapping or crash the VMM. Completing,
 * silently rejecting, or wedging the queue are all acceptable.
 *
 * Spec 5.13.6.5.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <string.h>

static test_result_t test_iommu_probe_addr_len_wrap(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    struct virtio_iommu_req_probe *req = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));

    req->head.type   = VIRTIO_IOMMU_T_PROBE;
    req->endpoint    = 0;
    req->tail.status = 0xFF;

    size_t in_len = (size_t)((uint8_t *)&req->properties - (uint8_t *)req);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), in_len,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, 0xFFFFFFFFFFFFF000ULL, 0x2000,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(I0111, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_probe_addr_len_wrap,
              "IOMMU probe response writable addr plus len wraps 64 bits",
              VIRTIO_SPEC_V1_2, "5.13.6.5");
