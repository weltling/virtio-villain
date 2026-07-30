/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0015: config_seg_max
 *
 * The device reports the maximum number of segments in a request in
 * seg_max. It must be a nonzero value the driver can rely on.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_cfg_seg_max(struct virtio_dev *dev,
                                           struct vring *vr)
{
    (void)vr;
    volatile struct virtio_scsi_config *cfg =
        (volatile struct virtio_scsi_config *)dev->device_cfg;
    __sync_synchronize();
    if (cfg->seg_max == 0)
        TFAIL("seg_max is 0");
    return TEST_PASS;
}

REGISTER_TEST(SCSI0015, VIRTIO_PCI_DEVICE_SCSI, test_scsi_cfg_seg_max,
              "Config seg_max is nonzero",
              VIRTIO_SPEC_V1_2, "5.6.4");
