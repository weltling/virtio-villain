/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0090: Secure erase segment count exceeds max_secure_erase_seg
 *
 * Spec 5.2.6.1: "VIRTIO_BLK_T_SECURE_ERASE requests MUST NOT
 * contain more than max_secure_erase_seg segments."
 *
 * Submit SECURE_ERASE with 256 segments (16*256=4096 bytes of data),
 * which should exceed any reasonable max_secure_erase_seg.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_blk_secure_erase_too_many(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    struct virtio_blk_discard_write_zeroes *segs = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_SECURE_ERASE;
    hdr->ioprio = 0;
    hdr->sector = 0;

    /* Fill 256 segments (4096 bytes = 1 page) */
    for (int i = 0; i < 256; i++) {
        segs[i].sector = i * 8;
        segs[i].num_sectors = 8;
        segs[i].flags = 0;
    }

    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t segs_phys = vv_virt_to_phys(segs);
    uint64_t status_phys = vv_virt_to_phys(status);

    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, segs_phys, 256 * sizeof(segs[0]),
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0090, VIRTIO_PCI_DEVICE_BLK, test_blk_secure_erase_too_many,
              "Secure erase with 256 segments (exceeds max_secure_erase_seg)",
              VIRTIO_SPEC_V1_2, "5.2.6.1");
