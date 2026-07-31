/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0199: eventq_four_buffers
 *
 * Post four writable buffers to the event queue and confirm the device
 * takes them without harm and stays operational.
 */
#include "tests/scsi/scsi_util.h"

#include <unistd.h>

static test_result_t test_scsi_eventq_four(struct virtio_dev *dev,
                                           struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);

    for (int k = 0; k < 4; k++) {
        vring_raw_set_desc(vr, k, vv_virt_to_phys(buf) + k * 64, 16,
                           VRING_DESC_F_WRITE, 0);
        vring_raw_set_avail(vr, k, (uint16_t)k);
    }
    vring_raw_set_avail_idx(vr, 4);
    virtio_pci_kick(dev, vr->queue);
    usleep(50000);

    __sync_synchronize();
    if (!(dev->common->device_status & VIRTIO_STATUS_DRIVER_OK))
        TFAIL("device_status 0x%02x lost DRIVER_OK",
              dev->common->device_status);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0199, VIRTIO_PCI_DEVICE_SCSI, test_scsi_eventq_four,
                "Event queue accepts four buffers and stays healthy",
                VIRTIO_SPEC_V1_4, "5.6.6.3", 1);
