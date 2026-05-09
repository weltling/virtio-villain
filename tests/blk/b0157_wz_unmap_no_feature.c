/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0157: blk_wz_unmap_no_feature
 *
 * Submit a WRITE_ZEROES request with the unmap flag set but the
 * VIRTIO_BLK_F_DISCARD feature is not negotiated by the harness.
 * Spec 5.2.6.2 says unmap is only valid when DISCARD is negotiated.
 * The device must reject or stay silent without crashing.
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

struct virtio_blk_discard_write_zeroes {
    uint64_t sector;
    uint32_t num_sectors;
    uint32_t flags;
} __attribute__((packed));

#define VIRTIO_BLK_T_WRITE_ZEROES 14
#define VIRTIO_BLK_WRITE_ZEROES_FLAG_UNMAP 0x1

static test_result_t test_blk_wz_unmap_no_feature(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    struct virtio_blk_discard_write_zeroes *seg = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_WRITE_ZEROES;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    seg->sector = 0;
    seg->num_sectors = 8;
    seg->flags = VIRTIO_BLK_WRITE_ZEROES_FLAG_UNMAP;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(seg), sizeof(*seg),
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(status), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0157, VIRTIO_PCI_DEVICE_BLK, test_blk_wz_unmap_no_feature,
              "WRITE_ZEROES unmap flag without DISCARD feature",
              VIRTIO_SPEC_V1_2, "5.2.6.2");
