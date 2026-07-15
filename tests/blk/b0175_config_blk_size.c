/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0175: read blk_size config field.
 *
 * Spec 5.2.4: When VIRTIO_BLK_F_BLK_SIZE is negotiated the device
 * config contains blk_size at offset 20. Read it and verify it is
 * a power of two between 512 and 65536 (typical sector sizes).
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <unistd.h>

/* blk_size is at config offset 20 (after capacity[8]+size_max[4]+seg_max[4]+geometry[4]) */
#define BLK_CFG_BLK_SIZE_OFFSET 20

static test_result_t test_blk_config_blk_size(struct virtio_dev *dev,
                                              struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_BLK_F_BLK_SIZE)))
        return TEST_SKIP;

    if (!dev->device_cfg || dev->device_cfg_length <= BLK_CFG_BLK_SIZE_OFFSET + 3)
        return TEST_SKIP;

    volatile uint32_t *blk_size = (volatile uint32_t *)
        ((char *)dev->device_cfg + BLK_CFG_BLK_SIZE_OFFSET);

    uint32_t val = *blk_size;

    /* Must be a power of two */
    if (val == 0 || (val & (val - 1)) != 0)
        TFAIL("blk_size %u is not a power of two", val);

    /* Sanity range: 512 to 64K */
    if (val < 512 || val > 65536)
        TFAIL("blk_size %u outside expected range 512..65536", val);

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(B0175, VIRTIO_PCI_DEVICE_BLK, test_blk_config_blk_size,
              "Read blk_size config field and validate range",
              VIRTIO_SPEC_V1_2, "5.2.4",
              (1ULL << VIRTIO_BLK_F_BLK_SIZE), 0);
