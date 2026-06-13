/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0064: Write zeroes data length not a multiple of 16
 *
 * Spec 5.2.6.1: "The length of data MUST be a multiple of the size
 * of struct virtio_blk_discard_write_zeroes" for WRITE_ZEROES.
 *
 * Submit WRITE_ZEROES with data length = 8 (half a segment struct).
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_blk_wz_data_not_16(struct virtio_dev *dev,
                                             struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_WRITE_ZEROES;
    hdr->ioprio = 0;
    hdr->sector = 0;
    memset(data, 0, 8);
    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);
    uint64_t status_phys = vv_virt_to_phys(status);

    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    /* 8 bytes — not a multiple of 16 */
    vring_raw_set_desc(vr, 1, data_phys, 8,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0064, VIRTIO_PCI_DEVICE_BLK, test_blk_wz_data_not_16,
              "Write zeroes data length not a multiple of 16 bytes",
              VIRTIO_SPEC_V1_2, "5.2.6.1");
