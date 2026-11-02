/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0145: read sector 0 then read at high sector
 *
 * Spec 5.2.5 defines sector addressing in 512 byte units. The
 * first and last addressable sectors should both be readable.
 * This test reads sector 0 then a sector near the disk end and
 * checks both complete with status OK. The default test disk is
 * a few MiB so sector 1023 is well within bounds.
 */
#include "lib/virtio_test.h"

static test_result_t one_read(struct virtio_dev *dev, struct vring *vr,
                              uint16_t base, uint16_t avail_slot,
                              uint64_t sector)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, sector);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);
    *st = 0xFF;

    vv_desc(vr, base, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, base + 1);
    vv_desc(vr, base + 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE,
            base + 2);
    vv_desc(vr, base + 2, st, 1, VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, avail_slot, base);
    vring_raw_set_avail_idx(vr, avail_slot + 1);

    VV_TRY(vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS));
    if (*st != 0)
        TFAIL("*st != 0");
    return TEST_PASS;
}

static test_result_t test_blk_endpoints(struct virtio_dev *dev,
                                        struct vring *vr)
{
    VV_TRY(one_read(dev, vr, 0, 0, 0));
    return one_read(dev, vr, 3, 1, 1023);
}

REGISTER_TEST(B0145, VIRTIO_PCI_DEVICE_BLK, test_blk_endpoints,
              "read sector 0 and high sector both succeed",
              VIRTIO_SPEC_V1_2, "5.2.5");
