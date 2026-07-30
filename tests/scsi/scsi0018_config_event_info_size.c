/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0018: config_event_info_size
 *
 * The device reports the largest event it will write to the event
 * queue in event_info_size. It must be a nonzero value so the driver
 * can size its event buffers.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_cfg_event_info_size(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    (void)vr;
    volatile struct virtio_scsi_config *cfg =
        (volatile struct virtio_scsi_config *)dev->device_cfg;
    __sync_synchronize();
    if (cfg->event_info_size == 0)
        TFAIL("event_info_size is 0");
    return TEST_PASS;
}

REGISTER_TEST(SCSI0018, VIRTIO_PCI_DEVICE_SCSI, test_scsi_cfg_event_info_size,
              "Config event_info_size is nonzero",
              VIRTIO_SPEC_V1_2, "5.6.4");
