/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0160: blk_write_zeroes_beyond_capacity
 *
 * Submit WRITE_ZEROES with sector beyond device capacity.
 * Spec 5.2.6.2 says sector must be within capacity. The device
 * must reject the request and return an error status without
 * writing to unallocated regions.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_wz_beyond(struct virtio_dev *dev,
                                        struct vring *vr)
{
    struct virtio_blk_outhdr *hdr =
        blk_hdr(VIRTIO_BLK_T_WRITE_ZEROES, 0xFFFFFFFFFFFFULL);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    /* WRITE_ZEROES has a discard/write_zeroes data segment */
    struct {
        uint64_t sector;
        uint32_t num_sectors;
        uint32_t flags;
    } __attribute__((packed)) *wz_data =
        (void *)((uint8_t *)hdr + sizeof(*hdr));
    wz_data->sector = 0xFFFFFFFFFFFFULL;
    wz_data->num_sectors = 8;
    wz_data->flags = 0;

    vv_desc(vr, 0, hdr, sizeof(*hdr) + 16, VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0160, VIRTIO_PCI_DEVICE_BLK, test_blk_wz_beyond,
              "WRITE_ZEROES sector beyond capacity",
              VIRTIO_SPEC_V1_2, "5.2.6.2");
