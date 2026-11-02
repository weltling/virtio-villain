/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0023: blk_simultaneous_kicks
 *
 * Rapid kicks on queue 0 with multiple pending requests.
 * Tests that the VMM handles rapid notification storms without race
 * condition crashes.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_simultaneous_kicks(struct virtio_dev *dev,
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

    /*
     * Rapidly kick the queue many times without waiting for completion.
     * This simulates a notification storm that may trigger races in
     * the VMM's async I/O handling.
     */
    uint16_t before = vr->used->idx;
    __sync_synchronize();

    for (int i = 0; i < 1000; i++)
        virtio_pci_kick(dev, 0);

    int elapsed = 0;
    int step = 10000;
    while (elapsed < VV_TIMEOUT_MS * 1000) {
        usleep(step);
        __sync_synchronize();
        if (vr->used->idx != before)
            return TEST_PASS;
        elapsed += step;
    }

    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    __sync_synchronize();
    uint8_t dev_status = cfg->device_status;
    if (dev_status == 0)
        TWEDGED("dev_status == 0");
    TREJECT("no device response within timeout");
}

REGISTER_TEST(B0023, VIRTIO_PCI_DEVICE_BLK, test_blk_simultaneous_kicks,
              "Rapid notification storm on queue",
              VIRTIO_SPEC_V1_2, "5.2.6");
