/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0067: desc_write_and_indirect_flags
 *
 * Set both WRITE and INDIRECT flags on a non-first descriptor in a
 * chain. The spec says INDIRECT must only appear on the first
 * descriptor and cannot be combined with other chain semantics.
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

static test_result_t test_desc_write_indirect(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);
    uint64_t status_phys = vv_virt_to_phys(status);

    /* First descriptor: normal header */
    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);

    /* Second descriptor: WRITE | INDIRECT (illegal combination) */
    vring_raw_set_desc(vr, 1, data_phys, 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE |
                       VRING_DESC_F_INDIRECT, 2);

    /* Third descriptor: normal status */
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0067, VIRTIO_PCI_DEVICE_BLK, test_desc_write_indirect,
              "WRITE|INDIRECT flags on non-first descriptor",
              VIRTIO_SPEC_V1_2, "2.7.5");
