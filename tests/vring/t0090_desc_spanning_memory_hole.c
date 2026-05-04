/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0090: Set descriptor addr+len to span across a memory hole.
 *
 * Spec 2.7.5: Descriptor addresses must reference valid guest memory.
 * Set addr to a valid page but len large enough to extend way beyond
 * guest RAM end. The device must not crash when accessing invalid
 * physical addresses.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} __attribute__((packed));

#define VIRTIO_BLK_T_IN 0

static test_result_t test_desc_spanning_hole(struct virtio_dev *dev,
                                             struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    /* Allocate a page for the data buffer start */
    uint8_t *data = vv_alloc_pages(1);
    uint64_t data_phys = vv_virt_to_phys(data);

    /* Header descriptor (valid) */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);

    /*
     * Data descriptor: starts at a valid page but claims 256MB length,
     * which will span well beyond the end of guest RAM into a memory
     * hole. The device must handle this gracefully.
     */
    vring_raw_set_desc(vr, 1, data_phys, 256 * 1024 * 1024,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);

    /* Status descriptor (valid) */
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(status), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0090, VIRTIO_PCI_DEVICE_BLK, test_desc_spanning_hole,
              "Descriptor addr+len spans across memory hole beyond RAM",
              VIRTIO_SPEC_V1_2, "2.7.5");
