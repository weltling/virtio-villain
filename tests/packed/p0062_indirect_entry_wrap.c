/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0062: wrapping descriptor entry inside a packed indirect table.
 *
 * Packed ring counterpart of T0129. A single packed ring slot points at
 * an indirect table with the F_INDIRECT flag, and one table entry has an
 * addr plus len that wraps past 2^64. The device reads the table and
 * processes the wrapping entry through the indirect read path. The data
 * entry base sits near the top of the address space and the length makes
 * addr plus len wrap to a low value. The device must reject the range or
 * stay alive.
 *
 * Spec 2.8.6.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/vring_packed.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_packed_indirect_entry_wrap(struct virtio_dev *dev,
                                                     struct vring_packed *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    /*
     * Indirect table: header valid, data entry wraps past 2^64
     * (base 2^64 - 4096, len 0x2000 -> end 0x1000), status valid.
     */
    struct vring_packed_desc *indirect = vv_alloc_pages(1);
    indirect[0].addr = vv_virt_to_phys(hdr);
    indirect[0].len = sizeof(*hdr);
    indirect[0].id = 0;
    indirect[0].flags = VRING_PACKED_DESC_F_NEXT;
    indirect[1].addr = 0xFFFFFFFFFFFFF000ULL;
    indirect[1].len = 0x2000;
    indirect[1].id = 0;
    indirect[1].flags = VRING_PACKED_DESC_F_NEXT | VRING_PACKED_DESC_F_WRITE;
    indirect[2].addr = vv_virt_to_phys(status);
    indirect[2].len = 1;
    indirect[2].id = 0;
    indirect[2].flags = VRING_PACKED_DESC_F_WRITE;

    vring_packed_set_desc(vr, 0, vv_virt_to_phys(indirect),
                          3 * sizeof(struct vring_packed_desc), 0,
                          VRING_PACKED_DESC_F_INDIRECT);

    uint8_t check_wrap = vr->wrap_counter;

    return vv_kick_and_wait_packed(dev, vr, vr->queue, 0,
                                   check_wrap, VV_TIMEOUT_MS);
}

REGISTER_TEST_PACKED(P0062, VIRTIO_PCI_DEVICE_BLK, test_packed_indirect_entry_wrap,
                     "Wrapping entry inside a packed indirect table",
                     VIRTIO_SPEC_V1_2, "2.8.6");
