/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0021: blk_secure_erase_without_feature
 *
 * Submit a SECURE ERASE request without having negotiated
 * VIRTIO_BLK_F_SECURE_ERASE. The device must return UNSUPP.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_secure_erase_no_feature(struct virtio_dev *dev,
                                                      struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    struct virtio_blk_discard_write_zeroes *seg = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_SECURE_ERASE;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    seg->sector = 0;
    seg->num_sectors = 8;
    seg->flags = 0;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t seg_phys = vv_virt_to_phys(seg);
    uint64_t status_phys = vv_virt_to_phys(status);

    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, seg_phys, sizeof(*seg),
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0021, VIRTIO_PCI_DEVICE_BLK, test_blk_secure_erase_no_feature,
              "SECURE ERASE without VIRTIO_BLK_F_SECURE_ERASE",
              VIRTIO_SPEC_V1_2, "5.2.6");
