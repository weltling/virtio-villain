/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0016: indirect_nested
 *
 * Set VRING_DESC_F_INDIRECT inside an indirect descriptor table.
 * The spec says a descriptor within an indirect table MUST NOT have
 * the INDIRECT flag set.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_indirect_nested(struct virtio_dev *dev,
                                          struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);
    struct vring_desc *indirect = vv_alloc_pages(1);
    struct vring_desc *nested = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);
    uint64_t status_phys = vv_virt_to_phys(status);
    uint64_t indirect_phys = vv_virt_to_phys(indirect);
    uint64_t nested_phys = vv_virt_to_phys(nested);

    /* Nested indirect table (should never be followed) */
    nested[0].addr = data_phys;
    nested[0].len = 512;
    nested[0].flags = VRING_DESC_F_NEXT | VRING_DESC_F_WRITE;
    nested[0].next = 1;
    nested[1].addr = status_phys;
    nested[1].len = 1;
    nested[1].flags = VRING_DESC_F_WRITE;
    nested[1].next = 0;

    /* Indirect table: first entry is header, second is INDIRECT (illegal) */
    indirect[0].addr = hdr_phys;
    indirect[0].len = sizeof(*hdr);
    indirect[0].flags = VRING_DESC_F_NEXT;
    indirect[0].next = 1;
    indirect[1].addr = nested_phys;
    indirect[1].len = 2 * sizeof(struct vring_desc);
    indirect[1].flags = VRING_DESC_F_INDIRECT; /* ILLEGAL nested indirect */
    indirect[1].next = 0;

    /* Main descriptor */
    vring_raw_set_desc(vr, 0, indirect_phys, 2 * sizeof(struct vring_desc),
                       VRING_DESC_F_INDIRECT, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0016, VIRTIO_PCI_DEVICE_BLK, test_indirect_nested,
              "INDIRECT flag inside indirect table",
              VIRTIO_SPEC_V1_2, "2.7.5.3");
