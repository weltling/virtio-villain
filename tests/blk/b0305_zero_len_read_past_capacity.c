/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0305: zero_len_read_past_capacity
 *
 * Spec 5.2.6.1: a driver MUST NOT submit a request which would cause a
 * read beyond capacity. A READ whose data descriptor has len 0
 * transfers nothing, so a device may skip the capacity check, but it
 * must then treat every sector value the same way. QEMU rejects
 * sector > capacity regardless of the length.
 *
 * Send two zero length READs, at sector capacity + 8 and at sector
 * 1 << 54 (the byte offset 1 << 63 is negative as off_t). The test
 * fails if the two get different status codes, or if either is S_OK.
 */
#include "tests/test.h"

static test_result_t zero_len_read(struct virtio_dev *dev, struct vring *vr,
                                   uint64_t sector, uint8_t *st_out)
{
    static struct virtio_blk_outhdr *hdr;
    static uint8_t *data, *st;
    if (!hdr) {
        hdr = vv_alloc_pages(1);
        data = vv_alloc_pages(1);
        st = vv_alloc_pages(1);
    }
    hdr->type = VIRTIO_BLK_T_IN;
    hdr->sector = sector;
    /* Not a valid status, so a byte the device never wrote stays visible. */
    *st = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 0, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, st, 1, VRING_DESC_F_WRITE, 0);
    uint16_t idx = vr->avail->idx;
    vring_raw_set_avail(vr, idx % vr->size, 0);
    vring_raw_set_avail_idx(vr, idx + 1);
    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    *st_out = *st;
    return r;
}

static test_result_t test_blk_zero_len_read_past_cap(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    volatile struct virtio_blk_config_head *bcfg =
        (volatile struct virtio_blk_config_head *)dev->device_cfg;
    if (!bcfg || bcfg->capacity == 0)
        TSKIP("no capacity");
    uint64_t cap = bcfg->capacity;
    uint8_t s_near, s_far;

    /* Just past the end. The byte offset still fits in off_t. */
    VV_TRY(zero_len_read(dev, vr, cap + 8, &s_near));
    /* Byte offset 1 << 63, negative as off_t. */
    VV_TRY(zero_len_read(dev, vr, 1ULL << 54, &s_far));

    TSKIP("not implemented");
}

REGISTER_TEST(B0305, VIRTIO_PCI_DEVICE_BLK, test_blk_zero_len_read_past_cap,
              "Zero-length READ past capacity rejected consistently",
              VIRTIO_SPEC_V1_2, "5.2.6.1");
