/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0195: write zeroes returns S_OK.
 *
 * Submit a valid WRITE_ZEROES for sectors 0..7 and verify the
 * device writes S_OK in the status byte. Positive path test for
 * the write zeroes command itself (B0170 already verifies the
 * data; this test focuses on the status response).
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_wz_status(struct virtio_dev *dev,
                                        struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_BLK_F_WRITE_ZEROES)))
        return TEST_SKIP;

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    struct virtio_blk_discard_write_zeroes *seg = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_WRITE_ZEROES; hdr->ioprio = 0; hdr->sector = 0;
    seg->sector = 0; seg->num_sectors = 8; seg->flags = 0;
    *st = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(seg), sizeof(*seg),
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;

    if (*st != VIRTIO_BLK_S_OK)
        TFAIL("write_zeroes status %u, expected S_OK", *st);

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(B0195, VIRTIO_PCI_DEVICE_BLK, test_blk_wz_status,
              "Write zeroes returns S_OK status",
              VIRTIO_SPEC_V1_2, "5.2.6.2",
              (1ULL << VIRTIO_BLK_F_WRITE_ZEROES), 0);
