/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Z0012: ZONE_RESET_ALL with nonzero sector field
 *
 * Spec 5.2.6.6 (v1.3): "In VIRTIO_BLK_T_ZONE_RESET_ALL request,
 * the driver MUST set the field sector to 0."
 *
 * Submit ZONE_RESET_ALL with sector = 0xDEAD to violate this.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>

#define VIRTIO_BLK_T_ZONE_RESET_ALL 32

struct virtio_blk_req {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
} __attribute__((packed));

static test_result_t test_zone_reset_all_nonzero(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    struct virtio_blk_req *hdr = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_ZONE_RESET_ALL;
    hdr->reserved = 0;
    hdr->sector = 0xDEAD; /* violates: MUST be 0 */
    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t status_phys = vv_virt_to_phys(status);

    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(Z0012, VIRTIO_PCI_DEVICE_BLK, test_zone_reset_all_nonzero,
              "ZONE_RESET_ALL with sector != 0 (MUST be zero)",
              VIRTIO_SPEC_V1_3, "5.2.6.6");
