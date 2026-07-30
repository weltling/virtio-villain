/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0016: config_max_sectors
 *
 * The device reports the maximum sectors per request in max_sectors.
 * It must be a nonzero value the driver can rely on.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_cfg_max_sectors(struct virtio_dev *dev,
                                               struct vring *vr)
{
    (void)vr;
    volatile struct virtio_scsi_config *cfg =
        (volatile struct virtio_scsi_config *)dev->device_cfg;
    __sync_synchronize();
    if (cfg->max_sectors == 0)
        TFAIL("max_sectors is 0");
    return TEST_PASS;
}

REGISTER_TEST(SCSI0016, VIRTIO_PCI_DEVICE_SCSI, test_scsi_cfg_max_sectors,
              "Config max_sectors is nonzero",
              VIRTIO_SPEC_V1_2, "5.6.4");
