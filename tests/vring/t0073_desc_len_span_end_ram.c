/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0073: desc_len_span_end_of_ram
 *
 * Set a descriptor's address and length so that the buffer ends
 * exactly at the last valid byte of guest RAM. This boundary case
 * is valid but tests whether the device's range check is off-by-one.
 */
#include "lib/virtio_test.h"

/* Assume 512 MiB RAM */
#define RAM_END 0x20000000ULL

static test_result_t test_desc_len_span_end_ram(struct virtio_dev *dev,
                                                struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    /* Data buffer ending at last byte of RAM */
    uint64_t data_addr = RAM_END - 512;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, data_addr, 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(T0073, VIRTIO_PCI_DEVICE_BLK, test_desc_len_span_end_ram,
              "Descriptor buffer spanning to exact end of RAM",
              VIRTIO_SPEC_V1_2, "2.7.5");
