/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0011: config_cdb_size
 *
 * The device reports the CDB buffer size it uses in the configuration
 * space. The default and the value QEMU uses is 32 bytes.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_cfg_cdb_size(struct virtio_dev *dev,
                                            struct vring *vr)
{
    (void)vr;
    volatile struct virtio_scsi_config *cfg =
        (volatile struct virtio_scsi_config *)dev->device_cfg;
    __sync_synchronize();
    if (cfg->cdb_size != VIRTIO_SCSI_CDB_SIZE)
        TFAIL("cdb_size %u, expected %u", cfg->cdb_size, VIRTIO_SCSI_CDB_SIZE);
    return TEST_PASS;
}

REGISTER_TEST(SCSI0011, VIRTIO_PCI_DEVICE_SCSI, test_scsi_cfg_cdb_size,
              "Config cdb_size is the default 32",
              VIRTIO_SPEC_V1_2, "5.6.4");
