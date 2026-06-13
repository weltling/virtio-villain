/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0087: Write with data larger than blk_size_max (if configured)
 *
 * Spec 5.2.5.2: device may set size_max. Submit a WRITE with a
 * data buffer of 65536 bytes (64KB) which may exceed size_max
 * if the device configured a smaller limit.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_blk_write_exceeds_size_max(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    /* 64KB = 16 pages */
    uint8_t *data = vv_alloc_pages(16);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_OUT;
    hdr->ioprio = 0;
    hdr->sector = 0;
    memset(data, 0x77, 65536);
    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);
    uint64_t status_phys = vv_virt_to_phys(status);

    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, data_phys, 65536,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0087, VIRTIO_PCI_DEVICE_BLK, test_blk_write_exceeds_size_max,
              "Write with 64KB data (may exceed blk_size_max)",
              VIRTIO_SPEC_V1_2, "5.2.5");
