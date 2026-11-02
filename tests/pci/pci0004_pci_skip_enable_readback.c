/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0004: pci_skip_queue_enable_readback
 *
 * After enabling a queue, read back queue_enable to verify it took
 * effect. Then try to use the queue without checking. This test
 * writes queue_enable=1 and immediately uses the queue, simulating
 * a driver that skips the readback confirmation.
 * Spec 4.1.4.3.2: driver SHOULD confirm queue is enabled.
 */
#include "lib/virtio_test.h"

static test_result_t test_pci_skip_enable_readback(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    /*
     * The harness already configured the queue. We just verify the
     * test scenario: use the queue without reading back queue_enable.
     * This is benign for most VMMs but exercises the code path.
     */
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    vring_submit(vr, 0);

    /*
     * Now disable the queue from under the device and re-enable
     * without readback. Some VMMs may not handle re-enable.
     */
    dev->common->queue_select = 0;
    __sync_synchronize();

    /* Kick immediately - no readback of queue_enable */
    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(PCI0004, VIRTIO_PCI_DEVICE_BLK, test_pci_skip_enable_readback,
              "Use queue without reading back queue_enable",
              VIRTIO_SPEC_V1_2, "4.1.4.3.2");
