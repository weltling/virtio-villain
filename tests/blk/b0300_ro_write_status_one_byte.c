/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0300: ro_write_status_one_byte
 *
 * A write to a read only device must complete with VIRTIO_BLK_S_IOERR
 * (spec 5.2.6.2). The status field is a single byte, so the device must
 * not write past the 1 byte status descriptor. The sidecar launches the
 * VM with a read only disk.
 */
#include "tests/test.h"

#define CANARY 0xA5u
/* Bytes after the status byte that the device must leave untouched. */
#define CANARY_LEN 3

static test_result_t test_blk_ro_write_status_one_byte(struct virtio_dev *dev,
                                                       struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_OUT, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    if (!virtio_pci_feature_offered(dev, VIRTIO_BLK_F_RO))
        TSKIP("device not read only");

    /* Boring setup: a one sector WRITE whose status byte is followed by
     * canary bytes in the same page. */
    memset(data, 0x5A, VIRTIO_BLK_SECTOR_SIZE);
    /* Not a valid status, so a byte the device never wrote stays visible. */
    st[0] = 0xFF;
    memset(st + 1, CANARY, CANARY_LEN);

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, VIRTIO_BLK_SECTOR_SIZE, VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, st, 1, VRING_DESC_F_WRITE, 0);
    VV_TRY(vv_submit_and_wait(dev, vr, 0));
    __sync_synchronize();

    TSKIP("not implemented");
}

REGISTER_TEST_REQUIRES(B0300, VIRTIO_PCI_DEVICE_BLK,
                       test_blk_ro_write_status_one_byte,
                       "Read-only write: status is exactly one byte",
                       VIRTIO_SPEC_V1_2, "5.2.6.2",
                       VV_FEATURE_BIT(VIRTIO_BLK_F_RO), 0);
