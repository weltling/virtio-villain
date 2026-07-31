/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0223: config_request_queue_count
 *
 * The runner advertises four request queues, so the configuration
 * num_queues reports 4.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_reqq_count(struct virtio_dev *dev,
                                          struct vring *vr)
{
    (void)vr;
    volatile struct virtio_scsi_config *cfg =
        (volatile struct virtio_scsi_config *)dev->device_cfg;
    __sync_synchronize();
    if (cfg->num_queues != 4)
        TFAIL("num_queues %u, expected 4", cfg->num_queues);
    return TEST_PASS;
}

REGISTER_TEST(SCSI0223, VIRTIO_PCI_DEVICE_SCSI, test_scsi_reqq_count,
              "Config reports four request queues",
              VIRTIO_SPEC_V1_4, "5.6.4");
