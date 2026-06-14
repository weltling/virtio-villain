/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0105: Descriptor with addr plus len overflowing 64 bits.
 *
 * Spec 2.7.5: addr and len define a guest physical buffer. A
 * range whose end addr plus len wraps past 2^64 is invalid.
 * Submit a block request with the data descriptor addr near
 * the top of the 64 bit address space and len large enough to
 * make addr + len wrap to a low value. A device that computes
 * the end address by addition without checking for overflow can
 * walk a wrapped range and access unrelated memory. The device
 * must reject the overflowing range.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_desc_addr_len_wrap(struct virtio_dev *dev,
                                             struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type   = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status     = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, 0xFFFFFFFFFFFFF000ULL, 0xFFFFFFFFu,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(status), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0105, VIRTIO_PCI_DEVICE_BLK, test_desc_addr_len_wrap,
              "Descriptor addr plus len overflowing 64 bits",
              VIRTIO_SPEC_V1_2, "2.7.5");
