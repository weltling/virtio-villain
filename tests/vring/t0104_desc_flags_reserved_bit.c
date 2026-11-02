/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0104: Descriptor flags with an undefined reserved bit set.
 *
 * Spec 2.7.5: Defined desc.flags bits are NEXT(1), WRITE(2),
 * INDIRECT(4). All other bits are reserved. Submit a chain
 * where the last descriptor's flags include bit 0x40 (reserved)
 * in addition to WRITE. The device must ignore the unknown bit
 * and process the descriptor normally rather than rejecting or
 * crashing on the unrecognised flag.
 */
#include "lib/virtio_test.h"

static test_result_t test_desc_flags_reserved_bit(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    /* Reserved bit 0x40 sneaked in beside WRITE. */
    vv_desc(vr, 2, status, 1, (uint16_t)(VRING_DESC_F_WRITE | 0x40), 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(T0104, VIRTIO_PCI_DEVICE_BLK, test_desc_flags_reserved_bit,
              "Descriptor flags with reserved bit set",
              VIRTIO_SPEC_V1_2, "2.7.5");
