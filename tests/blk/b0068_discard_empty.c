/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0068: Discard with zero-length data (empty segment list)
 *
 * Spec 5.2.6.1: data must be a multiple of 16, but 0 is technically
 * a multiple of 16 — tests that the device handles an empty discard
 * segment list gracefully (no crash or hang).
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_blk_discard_empty(struct virtio_dev *dev,
                                            struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_DISCARD;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t status_phys = vv_virt_to_phys(status);

    /* header → status, no data descriptor at all (0 segments) */
    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0068, VIRTIO_PCI_DEVICE_BLK, test_blk_discard_empty,
              "Discard with no data descriptor (zero segments)",
              VIRTIO_SPEC_V1_2, "5.2.6.1");
