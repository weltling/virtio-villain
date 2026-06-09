/* SPDX-License-Identifier: Apache-2.0 */
/*
 * F0039: num_request_queues must not exceed exposed queues.
 *
 * v1.4 5.11.4: cfg->num_request_queues plus the hiprio queue
 * must equal num_queues advertised by the common cfg.
 */
#include "tests/test.h"

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    if (!dev->device_cfg || dev->device_cfg_length < 40)
        return TEST_SKIP;

    volatile uint32_t *nrq = (volatile uint32_t *)
        ((volatile uint8_t *)dev->device_cfg + 36);
    uint32_t reqq = *nrq;
    uint16_t total = cfg->num_queues;
    if (reqq + 1 > total)
        TFAIL("num_request_queues=%u + hiprio > num_queues=%u",
              reqq, total);
    return TEST_PASS;
}

REGISTER_TEST(F0039, VIRTIO_PCI_DEVICE_FS, test,
              "num_request_queues bounded by num_queues",
              VIRTIO_SPEC_V1_4, "5.11.4");
