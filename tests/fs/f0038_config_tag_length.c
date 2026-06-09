/* SPDX-License-Identifier: Apache-2.0 */
/*
 * F0038: config region is at least tag plus num_request_queues.
 *
 * v1.4 5.11.4: device config layout is { char tag[36],
 *   le32 num_request_queues }. Confirm device_cfg_length is at
 * least 40 bytes.
 */
#include "tests/test.h"

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    (void)vr;
    if (!dev->device_cfg) return TEST_SKIP;
    if (dev->device_cfg_length < 40)
        TFAIL("device_cfg_length=%u below required 40",
              dev->device_cfg_length);
    return TEST_PASS;
}

REGISTER_TEST(F0038, VIRTIO_PCI_DEVICE_FS, test,
              "Config region holds tag and num_request_queues",
              VIRTIO_SPEC_V1_4, "5.11.4");
