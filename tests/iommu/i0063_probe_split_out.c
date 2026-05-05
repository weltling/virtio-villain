/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0063: PROBE response split across two writable descriptors.
 *
 * Place the request header in one read-only descriptor and the
 * response (properties + tail) in two writable descriptors
 * chained via NEXT. The spec lets the device write across any
 * descriptor chain layout, so the response must arrive without
 * crashing the device.
 *
 * Spec 5.13.6.5, 2.7.13.3.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_iommu_probe_split_out(struct virtio_dev *dev,
                                                struct vring *vr)
{
    struct virtio_iommu_req_probe *req = vv_alloc_pages(1);
    uint8_t *out2 = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));

    req->head.type   = VIRTIO_IOMMU_T_PROBE;
    req->endpoint    = 0;
    req->tail.status = 0xFF;

    uint64_t req_phys = vv_virt_to_phys(req);
    uint64_t out2_phys = vv_virt_to_phys(out2);
    size_t   in_len   = (size_t)((uint8_t *)&req->properties - (uint8_t *)req);
    size_t   half     = sizeof(req->properties) / 2;

    /* Header (read-only). */
    vring_raw_set_desc(vr, 0, req_phys, in_len,
                       VRING_DESC_F_NEXT, 1);
    /* First half of properties. */
    vring_raw_set_desc(vr, 1, req_phys + in_len, half,
                       VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 2);
    /* Second half of properties + tail in a different page. */
    vring_raw_set_desc(vr, 2, out2_phys,
                       (sizeof(req->properties) - half) + sizeof(req->tail),
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(I0063, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_probe_split_out,
              "Probe response split over two writable descriptors",
              VIRTIO_SPEC_V1_2, "5.13.6.5");
