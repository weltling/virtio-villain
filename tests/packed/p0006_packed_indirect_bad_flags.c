/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0006: packed_indirect_bad_flags
 *
 * In an indirect descriptor table, set flags other than WRITE and NEXT
 * (specifically, set the AVAIL and USED bits which are meaningless
 * inside indirect tables).
 * Spec 2.8.19: within indirect table, only WRITE and NEXT flags are
 * valid.
 */
#include "lib/virtio_test.h"

static test_result_t test_packed_indirect_bad_flags(struct virtio_dev *dev,
                                                    struct vring_packed *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    /* Build indirect table with illegal AVAIL/USED bits set */
    struct vring_packed_desc *ind = vv_alloc_pages(1);
    ind[0].addr = vv_virt_to_phys(hdr);
    ind[0].len = sizeof(*hdr);
    ind[0].id = 0;
    ind[0].flags = VRING_PACKED_DESC_F_NEXT |
                   VRING_PACKED_DESC_F_AVAIL | VRING_PACKED_DESC_F_USED;
    ind[1].addr = vv_virt_to_phys(data);
    ind[1].len = 512;
    ind[1].id = 1;
    ind[1].flags = VRING_PACKED_DESC_F_NEXT | VRING_PACKED_DESC_F_WRITE |
                   VRING_PACKED_DESC_F_AVAIL;
    ind[2].addr = vv_virt_to_phys(status);
    ind[2].len = 1;
    ind[2].id = 2;
    ind[2].flags = VRING_PACKED_DESC_F_WRITE | VRING_PACKED_DESC_F_USED;

    vring_packed_set_desc(vr, 0, vv_virt_to_phys(ind),
                          3 * sizeof(struct vring_packed_desc), 0,
                          VRING_PACKED_DESC_F_INDIRECT);
    vring_packed_advance(vr);

    return vv_kick_and_wait_packed(dev, vr, 0, 0, 1, VV_TIMEOUT_MS);
}

REGISTER_TEST_PACKED(P0006, VIRTIO_PCI_DEVICE_BLK, test_packed_indirect_bad_flags,
                     "AVAIL/USED flags set in indirect descriptor",
                     VIRTIO_SPEC_V1_2, "2.8.19");
