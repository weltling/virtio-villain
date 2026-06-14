/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0089: Set both WRITE and INDIRECT flags on the same descriptor.
 *
 * Spec 2.7.5.3.2: An INDIRECT descriptor changes the semantics of
 * the descriptor (addr points to a table, len is table size). The
 * WRITE flag is meaningless for an indirect descriptor and the
 * combination is not valid per spec 2.7.5. Test device behavior.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_indirect_write_flags(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    /* Build a valid indirect table */
    struct vring_desc *indirect = vv_alloc_pages(1);
    indirect[0].addr = vv_virt_to_phys(hdr);
    indirect[0].len = sizeof(*hdr);
    indirect[0].flags = VRING_DESC_F_NEXT;
    indirect[0].next = 1;
    indirect[1].addr = vv_virt_to_phys(data);
    indirect[1].len = 512;
    indirect[1].flags = VRING_DESC_F_NEXT | VRING_DESC_F_WRITE;
    indirect[1].next = 2;
    indirect[2].addr = vv_virt_to_phys(status);
    indirect[2].len = 1;
    indirect[2].flags = VRING_DESC_F_WRITE;
    indirect[2].next = 0;

    uint64_t indirect_phys = vv_virt_to_phys(indirect);

    /* Set INDIRECT | WRITE on the top-level descriptor (illegal) */
    vring_raw_set_desc(vr, 0, indirect_phys, 3 * sizeof(struct vring_desc),
                       VRING_DESC_F_INDIRECT | VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0089, VIRTIO_PCI_DEVICE_BLK, test_indirect_write_flags,
              "INDIRECT and WRITE flags combined on same descriptor",
              VIRTIO_SPEC_V1_2, "2.7.5");
