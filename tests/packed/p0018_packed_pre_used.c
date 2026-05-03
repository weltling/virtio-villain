/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0018: packed_all_descs_used_before_kick
 *
 * Mark all descriptors in the packed ring as "used" (set the used
 * flag bit matching device wrap counter) before the driver even makes
 * them available. Then make one available and kick. The device must
 * not be confused by pre-existing used flags on other slots.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/vring_packed.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} __attribute__((packed));

#define VIRTIO_BLK_T_IN 0

static test_result_t test_packed_all_used_before_kick(struct virtio_dev *dev,
                                                     struct vring_packed *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);
    uint64_t status_phys = vv_virt_to_phys(status);

    /*
     * Poison all descriptor slots by setting flags that look "used"
     * from the device's perspective (USED=1, AVAIL=0 when wrap=1).
     * The device wrap counter starts at 1, so used=1,avail=0 means "used."
     */
    uint16_t queue_size = vr->size;
    for (uint16_t i = 0; i < queue_size; i++) {
        vr->desc[i].addr = 0;
        vr->desc[i].len = 0;
        vr->desc[i].id = i;
        /* Set USED bit without AVAIL - looks already consumed */
        vr->desc[i].flags = (1 << 15); /* VRING_PACKED_DESC_F_USED */
    }
    __sync_synchronize();

    /* Now properly submit a 3-descriptor chain at slot 0 */
    vring_packed_set_desc(vr, 0, hdr_phys, sizeof(*hdr), 0,
                          VRING_PACKED_DESC_F_NEXT);
    vring_packed_set_desc(vr, 1, data_phys, 512, 1,
                          VRING_PACKED_DESC_F_NEXT | VRING_PACKED_DESC_F_WRITE);
    vring_packed_set_desc(vr, 2, status_phys, 1, 2,
                          VRING_PACKED_DESC_F_WRITE);

    return vv_kick_and_wait_packed(dev, vr, 0, 2, vr->wrap_counter, VV_TIMEOUT_MS);
}

REGISTER_TEST_PACKED(P0018, VIRTIO_PCI_DEVICE_BLK, test_packed_all_used_before_kick,
                     "All descriptors pre-marked used before first kick",
                     VIRTIO_SPEC_V1_2, "2.8.6");
