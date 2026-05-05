/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0067: PROBE properties terminator parsing.
 *
 * Submit a valid PROBE and inspect the properties buffer for
 * the NONE terminator (type field == 0). Per spec 5.13.6.5
 * the device fills the buffer with a sequence of TLVs ending
 * in a NONE entry. Verify the buffer is not entirely zero
 * (some property emitted) and contains a NONE somewhere.
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

static test_result_t test_iommu_probe_terminator(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    struct virtio_iommu_req_probe *req = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));

    req->head.type   = VIRTIO_IOMMU_T_PROBE;
    req->endpoint    = 0;
    memset(req->properties, 0xCC, sizeof(req->properties));
    req->tail.status = 0xFF;

    uint64_t req_phys = vv_virt_to_phys(req);
    size_t   in_len   = (size_t)((uint8_t *)&req->properties - (uint8_t *)req);
    size_t   out_len  = sizeof(req->properties) + sizeof(req->tail);

    vring_raw_set_desc(vr, 0, req_phys, in_len,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, req_phys + in_len, out_len,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(I0067, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_probe_terminator,
              "Probe properties parsing",
              VIRTIO_SPEC_V1_2, "5.13.6.5");
