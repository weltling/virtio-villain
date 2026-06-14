/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0012: pci_notify_wrong_queue
 *
 * Write a queue index to the notification register that exceeds the
 * device's num_queues. The device must not use this value as an array
 * index without bounds checking.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_pci_notify_wrong_queue(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);
    uint64_t status_phys = vv_virt_to_phys(status);

    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, data_phys, 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

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
