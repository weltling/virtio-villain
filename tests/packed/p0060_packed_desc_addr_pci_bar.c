/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0060: packed_desc_addr_pci_bar
 *
 * Packed ring version of T0063. A packed read request whose data
 * descriptor is device writable and addresses the device's own PCI BAR
 * MMIO region makes the device DMA disk bytes into its own registers.
 * The device must not DMA into its own registers regardless of the ring
 * layout. A hang or timeout is a guest triggered host denial of service.
 * Confirms whether the T0063 deadlock is reachable through the packed
 * pop path as well as the split path.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring_packed.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_packed_desc_addr_pci_bar(struct virtio_dev *dev,
                                                   struct vring_packed *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    uint64_t bar_addr = 0xFE000000ULL; /* typical PCI BAR region */

    uint16_t head = vr->next_avail;
    uint8_t wrap = vr->wrap_counter;
    vring_packed_set_desc(vr, head, vv_virt_to_phys(hdr), sizeof(*hdr),
                          head, VRING_PACKED_DESC_F_NEXT);
    vring_packed_advance(vr);
    /* Device writable data descriptor pointing into PCI BAR MMIO. */
    vring_packed_set_desc(vr, vr->next_avail, bar_addr, 512, head,
                          VRING_PACKED_DESC_F_NEXT |
                          VRING_PACKED_DESC_F_WRITE);
    vring_packed_advance(vr);
    vring_packed_set_desc(vr, vr->next_avail, vv_virt_to_phys(status),
                          1, head, VRING_PACKED_DESC_F_WRITE);
    vring_packed_advance(vr);

    return vv_kick_and_wait_packed(dev, vr, vr->queue, head, wrap,
                                   VV_TIMEOUT_MS);
}

REGISTER_TEST_PACKED(P0060, VIRTIO_PCI_DEVICE_BLK,
                     test_packed_desc_addr_pci_bar,
                     "Packed descriptor address pointing into PCI BAR MMIO",
                     VIRTIO_SPEC_V1_2, "2.7.5");
