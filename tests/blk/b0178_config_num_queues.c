/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0178: read num_queues config when VIRTIO_BLK_F_MQ negotiated.
 *
 * Spec 5.2.4: When VIRTIO_BLK_F_MQ is negotiated the config contains
 * num_queues at offset 36. It must match the device's actual queue
 * count (>= 1) and agree with common_cfg.num_queues.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <unistd.h>

/* num_queues at offset 36 in blk device config */
#define BLK_CFG_NUM_QUEUES_OFFSET 36

static test_result_t test_blk_config_num_queues(struct virtio_dev *dev,
                                                struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_BLK_F_MQ)))
        return TEST_SKIP;

    if (!dev->device_cfg ||
        dev->device_cfg_length <= BLK_CFG_NUM_QUEUES_OFFSET + 1)
        return TEST_SKIP;

    volatile uint16_t *nq = (volatile uint16_t *)
        ((char *)dev->device_cfg + BLK_CFG_NUM_QUEUES_OFFSET);

    uint16_t val = *nq;

    if (val == 0)
        TFAIL("num_queues is 0 despite MQ offered");

    /* Must agree with common config */
    uint16_t common_nq = cfg->num_queues;
    if (val != common_nq)
        TFAIL("device config num_queues %u != common num_queues %u",
              val, common_nq);

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(B0178, VIRTIO_PCI_DEVICE_BLK, test_blk_config_num_queues,
              "Read num_queues config and verify matches common",
              VIRTIO_SPEC_V1_2, "5.2.4",
              (1ULL << VIRTIO_BLK_F_MQ), 0);
