/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0014: config_num_queues
 *
 * The configuration num_queues counts the request queues. The total
 * virtqueue count reported in the common configuration adds the control
 * queue and the event queue, so it must be num_queues plus two.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_cfg_num_queues(struct virtio_dev *dev,
                                              struct vring *vr)
{
    (void)vr;
    volatile struct virtio_scsi_config *cfg =
        (volatile struct virtio_scsi_config *)dev->device_cfg;
    __sync_synchronize();
    uint32_t req_queues = cfg->num_queues;
    uint16_t total = dev->common->num_queues;
    if (req_queues == 0)
        TFAIL("num_queues is 0");
    if (total != req_queues + 2)
        TFAIL("common num_queues %u, expected %u", total, req_queues + 2);
    return TEST_PASS;
}

REGISTER_TEST(SCSI0014, VIRTIO_PCI_DEVICE_SCSI, test_scsi_cfg_num_queues,
              "Total virtqueues equal request queues plus control and event",
              VIRTIO_SPEC_V1_2, "5.6.2");
