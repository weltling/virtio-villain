/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0050: Packed indirect table containing a nested indirect entry.
 *
 * Spec 2.8.7: A descriptor with VRING_PACKED_DESC_F_INDIRECT set
 * must not appear inside an indirect descriptor table. Build an
 * indirect table whose single entry also carries the INDIRECT
 * flag and points back at the same table. A device that follows
 * the nested INDIRECT flag will recurse and may loop forever on
 * the self referential pointer. The device must reject the
 * nested indirect rather than walking into the loop.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring_packed.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_packed_indirect_nested(
        struct virtio_dev *dev, struct vring_packed *vr)
{
    struct vring_packed_desc *itab = vv_alloc_pages(1);
    memset(itab, 0, sizeof(*itab));
    uint64_t itab_phys = vv_virt_to_phys(itab);

    itab[0].addr  = itab_phys;
    itab[0].len   = sizeof(*itab);
    itab[0].id    = 0;
    itab[0].flags = VRING_PACKED_DESC_F_INDIRECT |
                    VRING_PACKED_DESC_F_WRITE;

    uint16_t phase = vr->wrap_counter ?
                     VRING_PACKED_DESC_F_AVAIL :
                     VRING_PACKED_DESC_F_USED;

    vr->desc[0].addr  = itab_phys;
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

REGISTER_TEST_PACKED(P0050, VIRTIO_PCI_DEVICE_BLK,
                     test_packed_indirect_nested,
                     "Packed indirect table with nested INDIRECT entry",
                     VIRTIO_SPEC_V1_2, "2.8.7");
