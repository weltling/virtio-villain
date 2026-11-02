/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0012: pci_notify_wrong_queue
 *
 * Write a queue index to the notification register that exceeds the
 * device's num_queues. The device must not use this value as an array
 * index without bounds checking.
 */
#include "lib/virtio_test.h"

static test_result_t test_pci_notify_wrong_queue(struct virtio_dev *dev,
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
     * Write an out-of-bounds queue index (0xFFFF) to the notify cap.
     * Then kick the real queue to see if the device is still alive.
     */
    __sync_synchronize();
    virtio_pci_kick(dev, 0xFFFF);
    usleep(50000);

    /* Now kick correctly and see if device processes the request */
    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(PCI0012, VIRTIO_PCI_DEVICE_BLK, test_pci_notify_wrong_queue,
              "Notify with queue index 0xFFFF (out of bounds)",
              VIRTIO_SPEC_V1_2, "4.1.4.9.2");
