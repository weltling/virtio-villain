/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0111: used ring len field matches bytes written for blk read.
 *
 * Spec 2.7.8: The device writes the number of bytes it has written
 * to memory into the used ring entry's len field. For a blk READ
 * of 512 bytes, the used len must be 512 + 1 (data + status byte).
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_used_len_verify(struct virtio_dev *dev,
                                          struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *st = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;

    /* Check used ring entry len */
    uint32_t used_len = vr->used->ring[0].len;

    /* Expected: 512 (data) + 1 (status) = 513 */
    if (used_len != 513)
        TFAIL("used len %u, expected 513 (512 data + 1 status)",
              used_len);

    return TEST_PASS;
}

REGISTER_TEST(T0111, VIRTIO_PCI_DEVICE_BLK, test_used_len_verify,
              "Used ring len matches bytes written for blk read",
              VIRTIO_SPEC_V1_2, "2.7.8");
