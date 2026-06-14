/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0104: Descriptor flags with an undefined reserved bit set.
 *
 * Spec 2.7.5: Defined desc.flags bits are NEXT(1), WRITE(2),
 * INDIRECT(4). All other bits are reserved. Submit a chain
 * where the last descriptor's flags include bit 0x40 (reserved)
 * in addition to WRITE. The device must ignore the unknown bit
 * and process the descriptor normally rather than rejecting or
 * crashing on the unrecognised flag.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_desc_flags_reserved_bit(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    /* Reserved bit 0x40 sneaked in beside WRITE. */
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(status), 1,
                       (uint16_t)(VRING_DESC_F_WRITE | 0x40), 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0104, VIRTIO_PCI_DEVICE_BLK, test_desc_flags_reserved_bit,
              "Descriptor flags with reserved bit set",
              VIRTIO_SPEC_V1_2, "2.7.5");
