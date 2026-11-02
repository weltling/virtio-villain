/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0067: Write zeroes with unmap flag and unknown flags
 *
 * Spec 5.2.6.2: "The device MUST set the status byte to
 * VIRTIO_BLK_S_UNSUPP for discard, secure erase and write zeroes
 * commands if any unknown flag is set."
 *
 * Submit WRITE_ZEROES with flags = 0xFFFFFFFF (unmap + unknown bits).
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_wz_unknown_flags_all(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_WRITE_ZEROES, 0);
    struct virtio_blk_discard_write_zeroes *seg = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    seg->sector = 0;
    seg->num_sectors = 1;
    seg->flags = 0xFFFFFFFF; /* all bits set, most are unknown */

    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, seg, sizeof(*seg), VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0067, VIRTIO_PCI_DEVICE_BLK, test_blk_wz_unknown_flags_all,
              "Write zeroes with all flag bits set (unknown flags)",
              VIRTIO_SPEC_V1_2, "5.2.6.2");
