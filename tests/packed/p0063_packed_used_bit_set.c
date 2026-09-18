/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0063: packed_used_bit_set
 *
 * The USED bit of a packed descriptor is device owned. The driver
 * only sets the AVAIL bit to offer a descriptor; the device flips
 * USED to match its wrap phase when it completes the chain. Here the
 * guest forges the USED bit on the head of an available chain, as if
 * the descriptor were already completed. A device must key completion
 * off the AVAIL bit against its own internal wrap counter, not trust
 * the guest written USED bit. If it does trust it, it may skip the
 * descriptor forever or double process it. Either way it must not
 * wedge the queue.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/vring_packed.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_packed_used_bit_set(struct virtio_dev *dev,
                                              struct vring_packed *vr)
{
    if (vr->size < 3)
        return TEST_SKIP;

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    uint16_t a0 = vr->next_avail;
    uint8_t wrap = vr->wrap_counter;
    uint16_t avail = wrap ? VRING_PACKED_DESC_F_AVAIL : 0;

    /* Head descriptor with the USED bit forged by the guest. */
    uint16_t i0 = a0;
    vr->desc[i0].addr = vv_virt_to_phys(hdr);
    vr->desc[i0].len = sizeof(*hdr);
    vr->desc[i0].id = a0;
    __sync_synchronize();
    vr->desc[i0].flags = avail | VRING_PACKED_DESC_F_USED |
                         VRING_PACKED_DESC_F_NEXT;
    vring_packed_advance(vr);

    uint16_t i1 = vr->next_avail;
    avail = vr->wrap_counter ? VRING_PACKED_DESC_F_AVAIL : 0;
    vr->desc[i1].addr = vv_virt_to_phys(data);
    vr->desc[i1].len = 512;
    vr->desc[i1].id = a0;
    __sync_synchronize();
    vr->desc[i1].flags = avail | VRING_PACKED_DESC_F_NEXT |
                         VRING_PACKED_DESC_F_WRITE;
    vring_packed_advance(vr);

    uint16_t i2 = vr->next_avail;
    avail = vr->wrap_counter ? VRING_PACKED_DESC_F_AVAIL : 0;
    vr->desc[i2].addr = vv_virt_to_phys(status);
    vr->desc[i2].len = 1;
    vr->desc[i2].id = a0;
    __sync_synchronize();
    vr->desc[i2].flags = avail | VRING_PACKED_DESC_F_WRITE;
    vring_packed_advance(vr);

    __sync_synchronize();
    virtio_pci_kick(dev, vr->queue);

    int elapsed = 0;
    while (elapsed < VV_TIMEOUT_MS * 1000) {
        usleep(10000);
        __sync_synchronize();
        if (*status != 0xFF)
            break;
        elapsed += 10000;
    }

    uint8_t dstat = dev->common->device_status;
    if (dstat == 0)
        TWEDGED("device reset after forged USED bit");
    if (*status == 0xFF)
        return TEST_REJECT; /* ignored the forged chain, still alive */
    if (*status == VIRTIO_BLK_S_OK)
        return TEST_PASS;   /* completed once despite the forged bit */
    TFAIL("unexpected status 0x%x", *status);
}

REGISTER_TEST_PACKED(P0063, VIRTIO_PCI_DEVICE_BLK, test_packed_used_bit_set,
                     "Packed head descriptor with guest forged USED bit",
                     VIRTIO_SPEC_V1_2, "2.8.4");
