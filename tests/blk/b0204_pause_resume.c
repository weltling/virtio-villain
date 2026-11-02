/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0204: virtio-blk pause and resume around a kick.
 *
 * The sidecar pauses the VM after the guest banner appears and
 * resumes it shortly after. The guest submits a read on sector 0,
 * then a second read on sector 1 once the resume window is
 * expected to have closed. The pause window must not corrupt the
 * device: both reads must complete with status OK.
 */
#include "lib/virtio_test.h"
#include "tests/blk/blk_mmio_helpers.h"

static test_result_t do_read(struct virtio_dev *dev, struct vring *vr,
                             uint64_t sector)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, sector);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);
    *st = 0xFF;
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, st, 1, VRING_DESC_F_WRITE, 0);
    uint16_t idx = vr->avail->idx;
    vring_raw_set_avail(vr, idx % vr->size, 0);
    vring_raw_set_avail_idx(vr, idx + 1);
    VV_TRY(vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS * 4));
    return *st == 0 ? TEST_PASS : TEST_FAIL;
}

static test_result_t test_blk_pause_resume(struct virtio_dev *dev,
                                           struct vring *vr)
{
    VV_TRY(do_read(dev, vr, 0));
    sleep(2);
    return do_read(dev, vr, 1);
}

REGISTER_TEST(B0204, VIRTIO_PCI_DEVICE_BLK, test_blk_pause_resume,
              "virtio-blk survives a pause and resume cycle",
              VIRTIO_SPEC_V1_2, "2.6");

REGISTER_MMIO_VARIANT(B0204, 2, blk_mmio_host_cycle);
