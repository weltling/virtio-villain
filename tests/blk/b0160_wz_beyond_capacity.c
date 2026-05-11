/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0160: blk_write_zeroes_beyond_capacity
 *
 * Submit WRITE_ZEROES with sector beyond device capacity.
 * Spec 5.2.6.2 says sector must be within capacity. The device
 * must reject the request and return an error status without
 * writing to unallocated regions.
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

#define VIRTIO_BLK_T_WRITE_ZEROES 13

static test_result_t test_blk_wz_beyond(struct virtio_dev *dev,
                                        struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_WRITE_ZEROES;
    hdr->ioprio = 0;
    hdr->sector = 0xFFFFFFFFFFFFULL;
    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t status_phys = vv_virt_to_phys(status);

    /* WRITE_ZEROES has a discard/write_zeroes data segment */
    struct {
        uint64_t sector;
        uint32_t num_sectors;
        uint32_t flags;
    } __attribute__((packed)) *wz_data =
        (void *)((uint8_t *)hdr + sizeof(*hdr));
    wz_data->sector = 0xFFFFFFFFFFFFULL;
    wz_data->num_sectors = 8;
    wz_data->flags = 0;

    vring_raw_set_desc(vr, 0, hdr_phys,
                       sizeof(*hdr) + 16,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0160, VIRTIO_PCI_DEVICE_BLK, test_blk_wz_beyond,
              "WRITE_ZEROES sector beyond capacity",
              VIRTIO_SPEC_V1_2, "5.2.6.2");
