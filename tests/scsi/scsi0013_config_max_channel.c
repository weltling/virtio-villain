/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0013: config_max_channel
 *
 * virtio-scsi supports only channel 0, so the device reports a
 * max_channel of 0 in the configuration space.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_cfg_max_channel(struct virtio_dev *dev,
                                               struct vring *vr)
{
    (void)vr;
    volatile struct virtio_scsi_config *cfg =
        (volatile struct virtio_scsi_config *)dev->device_cfg;
    __sync_synchronize();
    if (cfg->max_channel != 0)
        TFAIL("max_channel %u, expected 0", cfg->max_channel);
    return TEST_PASS;
}

REGISTER_TEST(SCSI0013, VIRTIO_PCI_DEVICE_SCSI, test_scsi_cfg_max_channel,
              "Config max_channel is 0",
              VIRTIO_SPEC_V1_2, "5.6.4");
