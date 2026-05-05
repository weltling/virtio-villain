/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0029: IOMMU PROBE for the first known endpoint.
 *
 * Submit PROBE for endpoint id 0 with a properly sized writable
 * properties area. The device must populate the properties with
 * a sequence of TLV descriptors terminated by NONE, with status
 * VIRTIO_IOMMU_S_OK.
 *
 * Spec 5.13.6.5.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_iommu_probe_known(struct virtio_dev *dev,
                                            struct vring *vr)
{
    struct virtio_iommu_req_probe *req = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));

    req->head.type   = VIRTIO_IOMMU_T_PROBE;
    req->endpoint    = 0;
    memset(req->properties, 0xAA, sizeof(req->properties));
    req->tail.status = 0xFF;

    uint64_t req_phys = vv_virt_to_phys(req);
    size_t   in_len   = (size_t)((uint8_t *)&req->properties - (uint8_t *)req);
    size_t   out_len  = sizeof(req->properties) + sizeof(req->tail);
    uint64_t out_phys = req_phys + in_len;

    vring_raw_set_desc(vr, 0, req_phys, in_len,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, out_phys, out_len,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(I0029, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_probe_known,
              "Probe a known endpoint and read properties",
              VIRTIO_SPEC_V1_2, "5.13.6.5");
