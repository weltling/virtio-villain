/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0012: config_sense_size
 *
 * The device reports the sense buffer size it uses in the
 * configuration space. The default and the value QEMU uses is 96 bytes.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_cfg_sense_size(struct virtio_dev *dev,
                                              struct vring *vr)
{
    (void)vr;
    volatile struct virtio_scsi_config *cfg =
        (volatile struct virtio_scsi_config *)dev->device_cfg;
    __sync_synchronize();
    if (cfg->sense_size != VIRTIO_SCSI_SENSE_SIZE)
        TFAIL("sense_size %u, expected %u",
              cfg->sense_size, VIRTIO_SCSI_SENSE_SIZE);
    return TEST_PASS;
}

REGISTER_TEST(SCSI0012, VIRTIO_PCI_DEVICE_SCSI, test_scsi_cfg_sense_size,
              "Config sense_size is the default 96",
              VIRTIO_SPEC_V1_2, "5.6.4");
