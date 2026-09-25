/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0081: desc_write_flag_wrong_out
 *
 * A blk write (device readable data) whose data descriptor carries the
 * WRITE flag, marking it device writable. The direction contradicts
 * the request type. The device must reject the mismatch rather than
 * writing into a buffer the driver meant to send.
 */
#include "tests/packed/packed_util.h"

static test_result_t test_packed_desc_write_flag_wrong_out(struct virtio_dev *dev,
                                                          struct vring_packed *vr)
{
    uint8_t *data = vv_alloc_pages(1);
    memset(data, 0xAB, 512);
    /* OUT request, but data descriptor is wrongly marked WRITE. */
    return pk_blk_chain(dev, vr, VIRTIO_BLK_T_OUT, vv_virt_to_phys(data),
                        512, VRING_PACKED_DESC_F_WRITE);
}

REGISTER_TEST_PACKED(P0081, VIRTIO_PCI_DEVICE_BLK,
                     test_packed_desc_write_flag_wrong_out,
                     "Packed OUT data descriptor wrongly marked writable",
                     VIRTIO_SPEC_V1_2, "2.8.6");
