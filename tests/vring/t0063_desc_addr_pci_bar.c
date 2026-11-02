/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0063: desc_addr_in_pci_bar
 *
 * Set a descriptor address pointing into the PCI BAR MMIO region
 * instead of guest RAM. The device must not DMA to/from its own
 * registers when processing this descriptor.
 */
#include "lib/virtio_test.h"

static test_result_t test_desc_addr_pci_bar(struct virtio_dev *dev,
                                            struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    /*
     * Use the BAR physical address (from dev->notify_base or similar)
     * as the data descriptor address. This points into PCI MMIO space.
     * We use a hardcoded high address typical for PCI BARs.
     */
    uint64_t bar_addr = 0xFE000000ULL; /* typical PCI BAR region */

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    /* Data descriptor points into PCI BAR MMIO space */
    vring_raw_set_desc(vr, 1, bar_addr, 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(T0063, VIRTIO_PCI_DEVICE_BLK, test_desc_addr_pci_bar,
              "Descriptor address pointing into PCI BAR MMIO region",
              VIRTIO_SPEC_V1_2, "2.7.5");
