/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0041: blk_kick_and_reset_race
 *
 * Submit a request and immediately reset the device without waiting
 * for completion. This creates a race between in-flight I/O processing
 * and device teardown. Tests for UAF/double-free in async backends.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_kick_reset_race(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    vring_submit(vr, 0);

    /* Kick to start I/O */
    virtio_pci_kick(dev, vr->queue);
    __sync_synchronize();

    /* Immediately reset without waiting for completion */
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    cfg->device_status = 0;
    __sync_synchronize();

    /* Brief pause for any async completions to race */
    usleep(50000);

    /* Device survived if we get here */
    return TEST_PASS;
}

REGISTER_TEST(B0041, VIRTIO_PCI_DEVICE_BLK, test_blk_kick_reset_race,
              "Kick then immediate reset (async teardown race)",
              VIRTIO_SPEC_V1_2, "3.3.1");
