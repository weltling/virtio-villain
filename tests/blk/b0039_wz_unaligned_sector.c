/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0039: blk_write_zeroes_unaligned_sector
 *
 * Submit a WRITE_ZEROES with an odd sector number (7) that is likely
 * not aligned to the device's preferred block size. Tests whether the
 * VMM validates alignment constraints without crashing.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_wz_unaligned(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    struct virtio_blk_discard_write_zeroes *seg = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_WRITE_ZEROES;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    /* Unaligned sector, odd count */
    seg->sector = 7;
    seg->num_sectors = 3;
    seg->flags = 0;

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

REGISTER_TEST(B0039, VIRTIO_PCI_DEVICE_BLK, test_blk_wz_unaligned,
              "WRITE_ZEROES at unaligned sector offset",
              VIRTIO_SPEC_V1_2, "5.2.6.4");
