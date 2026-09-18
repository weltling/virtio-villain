/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0130: cross_format_indirect
 *
 * A split ring descriptor with VRING_DESC_F_INDIRECT points at a
 * table that is laid out in packed descriptor format instead of split
 * format. The two layouts are both 16 bytes but order their fields
 * differently: split is addr, len, flags, next while packed is addr,
 * len, id, flags. A device reading this table as split indirect will
 * read the packed id field where it expects flags and the packed
 * flags field where it expects next, so the chain links and the
 * read/write bits are garbage. The device must reject or safely handle
 * the malformed table rather than follow a bogus next pointer or wedge.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/vring_packed.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_cross_format_indirect(struct virtio_dev *dev,
                                                struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_F_INDIRECT_DESC))
        return TEST_SKIP;

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);
    struct vring_packed_desc *table = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    /* Build a blk read chain, but in packed descriptor layout. */
    table[0].addr = vv_virt_to_phys(hdr);
    table[0].len = sizeof(*hdr);
    table[0].id = 0;
    table[0].flags = VRING_PACKED_DESC_F_NEXT;

    table[1].addr = vv_virt_to_phys(data);
    table[1].len = 512;
    table[1].id = 1;
    table[1].flags = VRING_PACKED_DESC_F_NEXT | VRING_PACKED_DESC_F_WRITE;

    table[2].addr = vv_virt_to_phys(status);
    table[2].len = 1;
    table[2].id = 2;
    table[2].flags = VRING_PACKED_DESC_F_WRITE;

    /* Split indirect descriptor pointing at the packed formatted table. */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(table),
                       3 * sizeof(struct vring_packed_desc),
                       VRING_DESC_F_INDIRECT, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_REQUIRES(T0130, VIRTIO_PCI_DEVICE_BLK, test_cross_format_indirect,
              "Split indirect table laid out in packed descriptor format",
              VIRTIO_SPEC_V1_2, "2.7.5.3",
              (1ULL << VIRTIO_F_INDIRECT_DESC), 0);
