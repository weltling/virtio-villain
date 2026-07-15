/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0176: read seg_max config field.
 *
 * Spec 5.2.4: When VIRTIO_BLK_F_SEG_MAX is negotiated the device
 * config contains seg_max at offset 12. Read it and verify it is
 * non zero (a device that advertises the feature must support at
 * least one segment per request).
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <unistd.h>

/* seg_max at offset 12 (after capacity[8]+size_max[4]) */
#define BLK_CFG_SEG_MAX_OFFSET 12

static test_result_t test_blk_config_seg_max(struct virtio_dev *dev,
                                             struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_BLK_F_SEG_MAX)))
        return TEST_SKIP;

    if (!dev->device_cfg || dev->device_cfg_length <= BLK_CFG_SEG_MAX_OFFSET + 3)
        return TEST_SKIP;

    volatile uint32_t *seg_max = (volatile uint32_t *)
        ((char *)dev->device_cfg + BLK_CFG_SEG_MAX_OFFSET);

    uint32_t val = *seg_max;

    if (val == 0)
        TFAIL("seg_max is 0 despite VIRTIO_BLK_F_SEG_MAX offered");

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(B0176, VIRTIO_PCI_DEVICE_BLK, test_blk_config_seg_max,
              "Read seg_max config field and verify non zero",
              VIRTIO_SPEC_V1_2, "5.2.4",
              (1ULL << VIRTIO_BLK_F_SEG_MAX), 0);
