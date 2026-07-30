/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0017: config_cmd_per_lun
 *
 * The device reports the suggested number of commands per logical unit
 * in cmd_per_lun. It must be a nonzero value the driver can rely on.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_cfg_cmd_per_lun(struct virtio_dev *dev,
                                               struct vring *vr)
{
    (void)vr;
    volatile struct virtio_scsi_config *cfg =
        (volatile struct virtio_scsi_config *)dev->device_cfg;
    __sync_synchronize();
    if (cfg->cmd_per_lun == 0)
        TFAIL("cmd_per_lun is 0");
    return TEST_PASS;
}

REGISTER_TEST(SCSI0017, VIRTIO_PCI_DEVICE_SCSI, test_scsi_cfg_cmd_per_lun,
              "Config cmd_per_lun is nonzero",
              VIRTIO_SPEC_V1_2, "5.6.4");
