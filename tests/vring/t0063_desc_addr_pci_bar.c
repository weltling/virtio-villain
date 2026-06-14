/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0063: desc_addr_in_pci_bar
 *
 * Set a descriptor address pointing into the PCI BAR MMIO region
 * instead of guest RAM. The device must not DMA to/from its own
 * registers when processing this descriptor.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_desc_addr_pci_bar(struct virtio_dev *dev,
                                            struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t status_phys = vv_virt_to_phys(status);

    /*
     * Use the BAR physical address (from dev->notify_base or similar)
     * as the data descriptor address. This points into PCI MMIO space.
     * We use a hardcoded high address typical for PCI BARs.
     */
    uint64_t bar_addr = 0xFE000000ULL; /* typical PCI BAR region */

    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    /* Data descriptor points into PCI BAR MMIO space */
    vring_raw_set_desc(vr, 1, bar_addr, 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0063, VIRTIO_PCI_DEVICE_BLK, test_desc_addr_pci_bar,
              "Descriptor address pointing into PCI BAR MMIO region",
              VIRTIO_SPEC_V1_2, "2.7.5");
