/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0122: desc_addr_lapic
 *
 * Companion to T0063. Instead of the device's own BAR, point the data
 * descriptor at the local APIC MMIO window at 0xFEE00000. A device
 * writable descriptor there makes the device DMA disk bytes into the
 * local APIC registers. The device must not DMA into platform MMIO. A
 * hang or timeout is a guest triggered host denial of service.
 */
#include "lib/virtio_test.h"

static test_result_t test_desc_addr_lapic(struct virtio_dev *dev,
                                          struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    uint64_t lapic_addr = 0xFEE00000ULL; /* local APIC MMIO window */

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    /* Device writable data descriptor pointing into the local APIC. */
    vring_raw_set_desc(vr, 1, lapic_addr, 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(T0122, VIRTIO_PCI_DEVICE_BLK, test_desc_addr_lapic,
              "Descriptor address pointing into the local APIC MMIO",
              VIRTIO_SPEC_V1_2, "2.7.5");
