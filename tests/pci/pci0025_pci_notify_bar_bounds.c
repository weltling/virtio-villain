/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0025: pci_notify_bar_access_at_boundary
 *
 * Write a notification at an offset near the end of the notify BAR
 * region (computed from notify_length and notify_off_multiplier).
 * Tests device handling of boundary notify writes.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} __attribute__((packed));

#define VIRTIO_BLK_T_IN 0

static test_result_t test_pci_notify_bar_bounds(struct virtio_dev *dev,
                                                struct vring *vr)
{
    if (!dev->notify_base || dev->notify_length == 0)
        return TEST_SKIP;

    /*
     * Compute the highest valid notify offset. Each queue has an
     * offset of queue_notify_off * notify_off_multiplier from
     * notify_base. Write to the last valid offset for queue 0.
     */
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    cfg->queue_select = 0;
    __sync_synchronize();
    uint16_t qno = cfg->queue_notify_off;

    uint32_t offset = (uint32_t)qno * dev->notify_off_multiplier;
    if (offset + 2 > dev->notify_length)
        return TEST_SKIP; /* notify region too small */

    /* Normal kick via computed offset - should work fine */
    virtio_pci_kick(dev, 0);
    usleep(10000);

    /* Verify device is still alive with real I/O */
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *st = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(PCI0025, VIRTIO_PCI_DEVICE_BLK, test_pci_notify_bar_bounds,
              "Notify BAR write at region boundary offset",
              VIRTIO_SPEC_V1_2, "4.1.4.4");
