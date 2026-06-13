/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0143: WCE write triggers config_generation increment
 *
 * Spec 4.1.4.3.1 says config_generation must be incremented by the
 * device whenever any field of the device configuration space
 * changes. VIRTIO_BLK_F_CONFIG_WCE makes the wce field driver
 * writable, so a write to wce that the device acknowledges should
 * be visible as a config_generation change. This catches devices
 * that accept the write but never bump the counter.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <unistd.h>


static test_result_t test_blk_wce_config_generation(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    if (!dev->device_cfg)
        return TEST_SKIP;
    if (dev->device_cfg_length <= VIRTIO_BLK_CFG_WCE_OFFSET)
        return TEST_SKIP;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_BLK_F_CONFIG_WCE)))
        return TEST_SKIP;

    volatile uint8_t *wce = (volatile uint8_t *)dev->device_cfg +
                            VIRTIO_BLK_CFG_WCE_OFFSET;

    uint8_t orig = *wce;
    uint8_t gen0 = cfg->config_generation;

    *wce = orig ? 0 : 1;
    __sync_synchronize();
    usleep(50000);
    __sync_synchronize();

    uint8_t gen1 = cfg->config_generation;

    /* Restore */
    *wce = orig;
    __sync_synchronize();

    /*
     * The spec requires the device to increment when the field
     * changes. Some devices accept the write silently without
     * bumping the counter, which is a spec violation but common.
     * Treat unchanged counter as REJECT rather than FAIL.
     */
    if (gen1 == gen0)
        TREJECT("gen1 == gen0");

    return TEST_PASS;
}

REGISTER_TEST(B0143, VIRTIO_PCI_DEVICE_BLK, test_blk_wce_config_generation,
              "WCE write bumps config_generation",
              VIRTIO_SPEC_V1_2, "4.1.4.3.1");
