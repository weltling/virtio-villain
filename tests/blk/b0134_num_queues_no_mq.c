/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0134: Read num_queues config without MQ feature.
 *
 * Access the num_queues configuration field without negotiating
 * VIRTIO_BLK_F_MQ. The device may return 0 or 1.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_blk_num_queues_no_mq(struct virtio_dev *dev,
                                               struct vring *vr)
{
    (void)vr;
    /*
     * The num_queues field is at offset 34 in the block device config.
     * Reading it without MQ negotiated: the result should be benign.
     */
    volatile uint8_t *cfg_space = (volatile uint8_t *)dev->device_cfg;
    uint16_t num_queues;
    memcpy(&num_queues, (void *)(cfg_space + 34), sizeof(num_queues));

    /*
     * Without MQ, device should either not expose this field or
     * return a safe default (1). Crash = fail.
     */
    return TEST_PASS;
}

REGISTER_TEST(B0134, VIRTIO_PCI_DEVICE_BLK, test_blk_num_queues_no_mq,
              "Read num_queues without MQ feature",
              VIRTIO_SPEC_V1_2, "5.2.4");
