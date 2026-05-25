/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0049: Packed indirect entry with phase bits set.
 *
 * Spec 2.8.6: Inside an indirect descriptor table the phase bits
 * VRING_PACKED_DESC_F_AVAIL and VRING_PACKED_DESC_F_USED do not
 * apply; their values are reserved. Build an indirect table
 * whose single entry has both phase bits set and dispatch from
 * an outer descriptor that uses the proper phase bits. The
 * device must follow the indirect entry as a plain descriptor
 * and ignore the spurious phase bits inside the table.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring_packed.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct packed_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t id;
    uint16_t flags;
} __attribute__((packed));

static test_result_t test_packed_indirect_phase_in_table(
        struct virtio_dev *dev, struct vring_packed *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0, 512);

    struct packed_desc *itab = vv_alloc_pages(1);
    memset(itab, 0, sizeof(*itab));
    itab[0].addr  = vv_virt_to_phys(buf);
    itab[0].len   = 512;
    itab[0].id    = 0;
    itab[0].flags = VRING_PACKED_DESC_F_WRITE |
                    VRING_PACKED_DESC_F_AVAIL |
                    VRING_PACKED_DESC_F_USED;

    uint16_t phase = vr->wrap_counter ?
                     VRING_PACKED_DESC_F_AVAIL :
                     VRING_PACKED_DESC_F_USED;

    vr->desc[0].addr  = vv_virt_to_phys(itab);
    vr->desc[0].len   = sizeof(*itab);
    vr->desc[0].id    = 0;
    vr->desc[0].flags = VRING_PACKED_DESC_F_INDIRECT | phase;
    __sync_synchronize();

    virtio_pci_kick(dev, vr->queue);
    usleep(VV_TIMEOUT_MS * 1000);

    if (dev->common->device_status == 0)
        TWEDGED("dev->common->device_status == 0");
    return TEST_PASS;
}

REGISTER_TEST_PACKED(P0049, VIRTIO_PCI_DEVICE_BLK,
                     test_packed_indirect_phase_in_table,
                     "Packed indirect entry with phase bits set",
                     VIRTIO_SPEC_V1_2, "2.8.6");
