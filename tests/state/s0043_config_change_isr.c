/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0043: config change interrupt and config_generation increment
 *
 * Spec 4.1.4.5 says when a device config field changes the device
 * sets bit 1 of ISR (config interrupt) and increments
 * config_generation. Force a config change via the WCE field
 * (requires VIRTIO_BLK_F_CONFIG_WCE), then read ISR and confirm
 * the config bit is set and the generation counter advanced.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <unistd.h>

static test_result_t test_config_change_isr(struct virtio_dev *dev,
                                            struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    if (!dev->isr || !dev->device_cfg)
        return TEST_SKIP;
    if (dev->device_cfg_length <= VIRTIO_BLK_CFG_WCE_OFFSET)
        return TEST_SKIP;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_BLK_F_CONFIG_WCE)))
        return TEST_SKIP;

    volatile uint8_t *wce = (volatile uint8_t *)dev->device_cfg +
                            VIRTIO_BLK_CFG_WCE_OFFSET;

    /* Drain ISR */
    (void)*dev->isr;
    __sync_synchronize();

    uint8_t orig = *wce;
    uint8_t gen0 = cfg->config_generation;

    *wce = orig ? 0 : 1;
    __sync_synchronize();
    usleep(50000);
    __sync_synchronize();

    uint8_t isr = *dev->isr;
    uint8_t gen1 = cfg->config_generation;

    /* Restore */
    *wce = orig;
    __sync_synchronize();

    int isr_set = (isr & VIRTIO_PCI_ISR_CONFIG) != 0;
    int gen_bumped = gen1 != gen0;

    /* Both signals are spec required for an actual change */
    if (!isr_set && !gen_bumped)
        TREJECT("!isr_set && !gen_bumped");
    if (!isr_set || !gen_bumped)
        TFAIL("!isr_set || !gen_bumped");

    return TEST_PASS;
}

REGISTER_TEST(S0043, VIRTIO_PCI_DEVICE_BLK, test_config_change_isr,
              "Config change sets ISR config bit and bumps config_generation",
              VIRTIO_SPEC_V1_2, "4.1.4.5");
