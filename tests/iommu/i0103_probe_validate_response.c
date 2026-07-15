/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0103: IOMMU probe validate response status and properties.
 *
 * Spec 5.13.6.5: PROBE returns S_OK and the properties area
 * contains TLV entries. Verify: (1) tail.status is S_OK, and
 * (2) the first property byte is either a valid type code
 * (RESV_MEM=1, etc.) or NONE=0 (empty properties). The device
 * must not leave the properties area unmodified.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <string.h>
#include <unistd.h>

#define VIRTIO_IOMMU_PROBE_T_NONE     0
#define VIRTIO_IOMMU_PROBE_T_RESV_MEM 1

static test_result_t test_iommu_probe_validate(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_iommu_req_probe *req = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));

    req->head.type = VIRTIO_IOMMU_T_PROBE;
    req->endpoint = 0;
    /* Fill properties with canary so we can detect writes */
    memset(req->properties, 0xFE, sizeof(req->properties));
    req->tail.status = 0xFF;

    uint64_t req_phys = vv_virt_to_phys(req);
    size_t in_len = (size_t)((uint8_t *)&req->properties - (uint8_t *)req);
    size_t out_len = sizeof(req->properties) + sizeof(req->tail);
    uint64_t out_phys = req_phys + in_len;

    vring_raw_set_desc(vr, 0, req_phys, in_len,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, out_phys, out_len,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;

    /* Verify status */
    if (req->tail.status != 0)
        TFAIL("probe status %u, expected 0 (S_OK)", req->tail.status);

    /* Verify properties were written (not all 0xFE canary) */
    int all_canary = 1;
    for (size_t i = 0; i < sizeof(req->properties); i++) {
        if (req->properties[i] != 0xFE) {
            all_canary = 0;
            break;
        }
    }

    /* If the device has no properties, it writes type=0 (NONE) at start */
    if (all_canary)
        TFAIL("properties area unchanged (all 0xFE canary)");

    /* First two bytes are type (le16); must be NONE or RESV_MEM */
    uint16_t first_type = req->properties[0] |
                          ((uint16_t)req->properties[1] << 8);
    if (first_type != VIRTIO_IOMMU_PROBE_T_NONE &&
        first_type != VIRTIO_IOMMU_PROBE_T_RESV_MEM)
        TFAIL("first property type %u unexpected", first_type);

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(I0103, VIRTIO_PCI_DEVICE_IOMMU,
              test_iommu_probe_validate,
              "Probe validates response status and property format",
              VIRTIO_SPEC_V1_2, "5.13.6.5",
              (1ULL << VIRTIO_IOMMU_F_PROBE), 0);
