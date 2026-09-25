/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0075: desc_addr_mmio_read_dir
 *
 * Packed twin of T0121. A write request whose device readable data
 * descriptor points into the PCI BAR MMIO region, so the device reads
 * its own registers as the payload to store. The device must not DMA
 * from platform or its own MMIO; a hang is a guest triggered host
 * denial of service.
 */
#include "tests/packed/packed_util.h"

static test_result_t test_packed_desc_addr_mmio_read_dir(struct virtio_dev *dev,
                                                        struct vring_packed *vr)
{
    /* Device readable data descriptor (no WRITE) into PCI BAR MMIO. */
    return pk_blk_chain(dev, vr, VIRTIO_BLK_T_OUT, 0xFE000000ULL, 512, 0);
}

REGISTER_TEST_PACKED(P0075, VIRTIO_PCI_DEVICE_BLK,
                     test_packed_desc_addr_mmio_read_dir,
                     "Packed readable descriptor reading from PCI BAR MMIO",
                     VIRTIO_SPEC_V1_2, "2.8.6");
