/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0101: eventq_accepts_buffer
 *
 * Post a writable buffer to the event queue and confirm the device
 * takes it without harm and stays operational, since no event is
 * generated the buffer simply waits.
 */
#include "tests/scsi/scsi_util.h"

#include <unistd.h>

static test_result_t test_scsi_eventq_buffer(struct virtio_dev *dev,
                                             struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(buf), 16,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    virtio_pci_kick(dev, vr->queue);
    usleep(50000);

    __sync_synchronize();
    if (!(dev->common->device_status & VIRTIO_STATUS_DRIVER_OK))
        TFAIL("device_status 0x%02x lost DRIVER_OK",
              dev->common->device_status);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0101, VIRTIO_PCI_DEVICE_SCSI, test_scsi_eventq_buffer,
                "Event queue accepts a buffer and the device stays healthy",
                VIRTIO_SPEC_V1_4, "5.6.6.3", 1);
