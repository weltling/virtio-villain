/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0029: Packed + EVENT_IDX interaction (spec 2.8.10)
 *
 * Enable EVENT_IDX feature and use packed ring notification
 * suppression with desc-based events. Submit a request with
 * event suppression enabled.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/vring_packed.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_packed_event_idx(struct virtio_dev *dev,
                                           struct vring_packed *vr)
{
    /*
     * Set the driver event suppression area to DESC mode with
     * a specific descriptor index to signal at.
     */
    if (!vr->driver_event)
        return TEST_SKIP;

    /* Set driver event suppression: DESC mode, signal at idx=0 */
    vr->driver_event->flags = VRING_PACKED_EVENT_FLAG_DESC;
    vr->driver_event->off_wrap = (0 & 0x7FFF) | ((uint16_t)vr->wrap_counter << 15);
    __sync_synchronize();

    /* Submit a normal request */
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *st = 0xFF;

    struct vring_packed_desc *ind = vv_alloc_pages(1);
    ind[0].addr = vv_virt_to_phys(hdr);
    ind[0].len = sizeof(*hdr);
    ind[0].id = 0;
    ind[0].flags = VRING_PACKED_DESC_F_NEXT;
    ind[1].addr = vv_virt_to_phys(data);
    ind[1].len = 512;
    ind[1].id = 0;
    ind[1].flags = VRING_PACKED_DESC_F_NEXT | VRING_PACKED_DESC_F_WRITE;
    ind[2].addr = vv_virt_to_phys(st);
    ind[2].len = 1;
    ind[2].id = 0;
    ind[2].flags = VRING_PACKED_DESC_F_WRITE;

    uint16_t idx = vr->next_avail;
    uint8_t wrap = vr->wrap_counter;

    uint16_t avail_flags = VRING_PACKED_DESC_F_INDIRECT;
    if (wrap)
        avail_flags |= VRING_PACKED_DESC_F_AVAIL;
    else
        avail_flags |= VRING_PACKED_DESC_F_USED;

    vr->desc[idx].addr = vv_virt_to_phys(ind);
    vr->desc[idx].len = 3 * sizeof(*ind);
    vr->desc[idx].id = 0;
    __sync_synchronize();
    vr->desc[idx].flags = avail_flags;
    __sync_synchronize();

    vr->next_avail = (idx + 1) % vr->size;
    if (vr->next_avail == 0)
        vr->wrap_counter ^= 1;

    virtio_pci_kick(dev, vr->queue);

    /* Wait for completion */
    int elapsed = 0;
    while (elapsed < VV_TIMEOUT_MS * 1000) {
        usleep(10000);
        __sync_synchronize();
        uint16_t flags = vr->desc[idx].flags;
        int used_bit = !!(flags & VRING_PACKED_DESC_F_USED);
        int avail_bit = !!(flags & VRING_PACKED_DESC_F_AVAIL);
        if (used_bit == avail_bit)
            return TEST_PASS;
        elapsed += 10000;
    }

    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    TREJECT("no device response within timeout");
}

REGISTER_TEST_PACKED(P0029, VIRTIO_PCI_DEVICE_BLK, test_packed_event_idx,
    "Packed ring with EVENT_IDX desc-based notification suppression",
    VIRTIO_SPEC_V1_2, "2.8.10");
