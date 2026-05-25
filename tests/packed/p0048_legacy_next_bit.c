/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0048: Packed descriptor with the legacy split NEXT bit set.
 *
 * Spec 2.8.6: In the packed ring format, bit 0 of desc.flags
 * (the split format NEXT flag) is unused. Submit a descriptor
 * carrying AVAIL/USED phase bits, WRITE, and additionally the
 * legacy NEXT bit. The device must process the descriptor by
 * its packed semantics and ignore the stray legacy bit rather
 * than misinterpreting the descriptor as a multi entry chain.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring_packed.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

#define PACKED_DESC_F_NEXT_LEGACY 0x0001u

static test_result_t test_packed_legacy_next_bit(struct virtio_dev *dev,
                                                 struct vring_packed *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0, 512);

    uint16_t avail = vr->wrap_counter ? VRING_PACKED_DESC_F_AVAIL : 0;
    uint16_t used  = vr->wrap_counter ? 0 : VRING_PACKED_DESC_F_USED;

    vr->desc[0].addr = vv_virt_to_phys(buf);
    vr->desc[0].len  = 512;
    vr->desc[0].id   = 0;
    __sync_synchronize();
    vr->desc[0].flags = (uint16_t)(avail | used |
                                   VRING_PACKED_DESC_F_WRITE |
                                   PACKED_DESC_F_NEXT_LEGACY);
    __sync_synchronize();

    virtio_pci_kick(dev, vr->queue);
    usleep(VV_TIMEOUT_MS * 1000);

    if (dev->common->device_status == 0)
        TWEDGED("device wedged after legacy NEXT bit on packed desc");
    return TEST_PASS;
}

REGISTER_TEST_PACKED(P0048, VIRTIO_PCI_DEVICE_BLK,
                     test_packed_legacy_next_bit,
                     "Packed descriptor with split format NEXT bit set",
                     VIRTIO_SPEC_V1_2, "2.8.6");
