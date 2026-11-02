/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0035: blk_write_data_wrong_direction
 *
 * Submit a WRITE (OUT) request but mark the data descriptor as
 * device-writable instead of device-readable. The device should
 * detect that the direction is wrong and not attempt to read from
 * a buffer the guest expects the device to write into.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_write_wrong_dir(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_OUT, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    memset(data, 0xAA, 512);
    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t status_phys = vv_virt_to_phys(status);

    /*
     * Header is readable (correct), data is WRITE (wrong for OUT),
     * status is writable (correct).
     */
    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0035, VIRTIO_PCI_DEVICE_BLK, test_blk_write_wrong_dir,
              "WRITE with device-writable data descriptor (wrong direction)",
              VIRTIO_SPEC_V1_2, "5.2.6.1");
