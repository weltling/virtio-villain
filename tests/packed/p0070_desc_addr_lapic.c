/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0070: desc_addr_lapic
 *
 * Packed twin of T0122. Point a device writable data descriptor at the
 * local APIC MMIO window at 0xFEE00000. A device that DMAs disk bytes
 * there writes into platform MMIO from inside I/O completion. The
 * device must not DMA into platform MMIO; a hang is a guest triggered
 * host denial of service.
 */
#include "lib/virtio_test.h"
#include "tests/packed/packed_util.h"

static test_result_t test_packed_desc_addr_lapic(struct virtio_dev *dev,
                                                struct vring_packed *vr)
{
    return pk_blk_chain(dev, vr, VIRTIO_BLK_T_IN, 0xFEE00000ULL, 512,
                        VRING_PACKED_DESC_F_WRITE);
}

REGISTER_TEST_PACKED(P0070, VIRTIO_PCI_DEVICE_BLK,
                     test_packed_desc_addr_lapic,
                     "Packed descriptor address into the local APIC MMIO",
                     VIRTIO_SPEC_V1_2, "2.8.6");
