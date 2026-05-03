/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0074: avail_all_entries_same_desc
 *
 * Fill the available ring with entries all pointing to the same
 * descriptor index. The device must handle repeated head indices
 * without aliasing internal state or double-completing.
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

static test_result_t test_avail_all_same_desc(struct virtio_dev *dev,
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

    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, data_phys, 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    /* Post the same head (0) into 8 avail ring slots */
    for (uint16_t i = 0; i < 8; i++)
        vring_raw_set_avail(vr, i, 0);
    vring_raw_set_avail_idx(vr, 8);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0074, VIRTIO_PCI_DEVICE_BLK, test_avail_all_same_desc,
              "All avail ring entries point to same descriptor",
              VIRTIO_SPEC_V1_2, "2.7.5");
