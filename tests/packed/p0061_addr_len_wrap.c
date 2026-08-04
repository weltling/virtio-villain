/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0061: packed data descriptor whose addr plus len wraps 2^64.
 *
 * Packed ring counterpart of T0105. Submit a block read whose data
 * descriptor base sits near the top of the address space with a length
 * that makes addr plus len wrap to a low value. A device that computes
 * the end with a plain addition gets a small wrapped result and a naive
 * bounds check passes, so it may walk a wrapped range. The device must
 * reject the range or stay alive.
 *
 * Spec 2.8.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring_packed.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_packed_addr_len_wrap(struct virtio_dev *dev,
                                               struct vring_packed *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type   = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status     = 0xFF;

    vring_packed_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr), 0,
                          VRING_PACKED_DESC_F_NEXT);
    vring_packed_advance(vr);
    vring_packed_set_desc(vr, 1, 0xFFFFFFFFFFFFF000ULL, 0x2000, 1,
                          VRING_PACKED_DESC_F_NEXT | VRING_PACKED_DESC_F_WRITE);
    vring_packed_advance(vr);
    vring_packed_set_desc(vr, 2, vv_virt_to_phys(status), 1, 2,
                          VRING_PACKED_DESC_F_WRITE);
    vring_packed_advance(vr);

    return vv_kick_and_wait_packed(dev, vr, 0, 0, 1, VV_TIMEOUT_MS);
}

REGISTER_TEST_PACKED(P0061, VIRTIO_PCI_DEVICE_BLK, test_packed_addr_len_wrap,
                     "Packed data descriptor addr plus len wraps 64 bits",
                     VIRTIO_SPEC_V1_2, "2.8");
