/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0121: desc_addr_mmio_read_dir
 *
 * Companion to T0063. T0063 makes the device write disk data into MMIO
 * with a device writable descriptor. This one reverses the DMA
 * direction: a write request whose data descriptor is device readable
 * and points into the PCI BAR MMIO region, so the device reads its own
 * registers as the payload to store. The device must not DMA from its
 * own registers. A hang or timeout is a guest triggered host denial of
 * service.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_desc_addr_mmio_read_dir(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_OUT;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t status_phys = vv_virt_to_phys(status);
    uint64_t bar_addr = 0xFE000000ULL; /* typical PCI BAR region */

    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    /* Device readable data descriptor pointing into PCI BAR MMIO. */
    vring_raw_set_desc(vr, 1, bar_addr, 512,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0121, VIRTIO_PCI_DEVICE_BLK, test_desc_addr_mmio_read_dir,
              "Readable descriptor reading from PCI BAR MMIO region",
              VIRTIO_SPEC_V1_2, "2.7.5");
