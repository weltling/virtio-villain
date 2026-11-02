/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0049: blk_get_id_scattered_buffer
 *
 * Issue a GET_ID command with the ID response buffer split across
 * multiple chained descriptors (scattered). Tests device handling
 * of multi-descriptor writable buffers for identification.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_get_id_scattered(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_GET_ID, 0);
    uint8_t *id_part1 = vv_alloc_pages(1);
    uint8_t *id_part2 = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    /* Split 20-byte ID buffer across two 10-byte descriptors */
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, id_part1, 10, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, id_part2, 10, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 3);
    vv_desc(vr, 3, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0049, VIRTIO_PCI_DEVICE_BLK, test_blk_get_id_scattered,
              "GET_ID with response buffer split across two descriptors",
              VIRTIO_SPEC_V1_2, "5.2.6.1");
