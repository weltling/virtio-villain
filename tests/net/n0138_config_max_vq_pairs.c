/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0138: read max_virtqueue_pairs from net config.
 *
 * Spec 5.1.4: When VIRTIO_NET_F_MQ is negotiated, the device config
 * contains max_virtqueue_pairs at offset 8. The value must be at
 * least 1 and not exceed 0x8000.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <unistd.h>

#define NET_CFG_MAX_VQ_PAIRS_OFFSET 8

static test_result_t test_net_config_max_vq_pairs(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_NET_F_MQ)))
        return TEST_SKIP;

    if (!dev->device_cfg || dev->device_cfg_length <= NET_CFG_MAX_VQ_PAIRS_OFFSET + 1)
        return TEST_SKIP;

    volatile uint16_t *pairs = (volatile uint16_t *)
        ((char *)dev->device_cfg + NET_CFG_MAX_VQ_PAIRS_OFFSET);

    uint16_t val = *pairs;

    if (val == 0)
        TFAIL("max_virtqueue_pairs is 0");
    if (val > 0x8000)
        TFAIL("max_virtqueue_pairs %u exceeds 0x8000", val);

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(N0138, VIRTIO_PCI_DEVICE_NET, test_net_config_max_vq_pairs,
              "Read max_virtqueue_pairs config and validate",
              VIRTIO_SPEC_V1_2, "5.1.4",
              (1ULL << VIRTIO_NET_F_MQ), 0);
