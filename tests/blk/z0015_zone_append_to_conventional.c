/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Z0015: zone_append_to_conventional
 *
 * Submit a ZONE_APPEND targeting sector 0, which on a non zoned
 * default device is not part of any sequential zone. Spec v1.3
 * 5.2.6 says ZONE_APPEND must return UNSUPP or IOERR when
 * targeting a conventional or absent zone. The device must
 * remain alive.
 */
#include "lib/virtio_test.h"

static test_result_t test_zone_append_conv(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_ZONE_APPEND, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    memset(data, 0x33, 512);
    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(Z0015, VIRTIO_PCI_DEVICE_BLK, test_zone_append_conv,
              "ZONE_APPEND to a conventional or absent zone",
              VIRTIO_SPEC_V1_3, "5.2.6");
