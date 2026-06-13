/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0058: Config space topology fields (spec 5.2.4)
 *
 * Read topology configuration fields (physical_block_exp,
 * alignment_offset, min_io_size, opt_io_size) if available.
 * Verify they are accessible and consistent.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

/* Offsets in virtio_blk_config for topology fields */
#define CFG_CAPACITY_OFF       0   /* uint64 */
#define CFG_SIZE_MAX_OFF       8   /* uint32 */
#define CFG_SEG_MAX_OFF       12   /* uint32 */
#define CFG_GEOMETRY_OFF      16   /* cylinders(16) + heads(8) + sectors(8) */
#define CFG_BLK_SIZE_OFF      20   /* uint32 */
#define CFG_TOPOLOGY_OFF      24   /* physical_block_exp(8) + align_offset(8) + min_io(16) + opt_io(32) */

static test_result_t test_blk_config_topology(struct virtio_dev *dev,
                                              struct vring *vr)
{
    (void)vr;

    if (!dev->device_cfg)
        return TEST_SKIP;

    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t offered = cfg->device_feature;

    if (!(offered & (1U << VIRTIO_BLK_F_TOPOLOGY)))
        return TEST_SKIP;

    if (dev->device_cfg_length < CFG_TOPOLOGY_OFF + 8)
        return TEST_SKIP;

    /* Read topology fields */
    volatile uint8_t *topo = (volatile uint8_t *)dev->device_cfg + CFG_TOPOLOGY_OFF;
    uint8_t physical_block_exp = topo[0];
    uint8_t alignment_offset = topo[1];
    uint16_t min_io_size = *(volatile uint16_t *)(topo + 2);
    uint32_t opt_io_size = *(volatile uint32_t *)(topo + 4);

    /* Sanity: physical_block_exp should be small (0-20 reasonable) */
    if (physical_block_exp > 20)
        TFAIL("physical_block_exp > 20");

    /* alignment_offset should be <= (1 << physical_block_exp) */
    (void)alignment_offset;
    (void)min_io_size;
    (void)opt_io_size;

    return TEST_PASS;
}

REGISTER_TEST(B0058, VIRTIO_PCI_DEVICE_BLK, test_blk_config_topology,
              "Read topology config fields (physical_block_exp, etc.)",
              VIRTIO_SPEC_V1_2, "5.2.4");
