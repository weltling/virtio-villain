/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0023: pci_isr_config_change_bit
 *
 * Write to a device config field (if available) and check whether
 * the ISR config-change bit is set. The device should signal
 * config changes through ISR bit 1.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} __attribute__((packed));

#define VIRTIO_BLK_T_IN 0

static test_result_t test_pci_isr_config_change(struct virtio_dev *dev,
                                                struct vring *vr)
{
    (void)vr;

    if (!dev->isr)
        return TEST_SKIP;
    if (!dev->device_cfg)
        return TEST_SKIP;

    /* Clear pending ISR */
    (void)*dev->isr;
    __sync_synchronize();

    /*
     * For virtio-blk, reading config_generation triggers a config
     * read cycle. We can't easily force a config change from guest
     * side, so just verify the ISR register is accessible and that
     * the config-change bit is not spuriously set after a read.
     */
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    (void)cfg->config_generation;
    __sync_synchronize();
    usleep(10000);

    uint8_t isr = *dev->isr;

    /*
     * The config-change bit should NOT be set just from reading
     * config_generation — only the device can trigger it when
     * config actually changes.
     */
    if (isr & VIRTIO_PCI_ISR_CONFIG)
        TFAIL("isr & VIRTIO_PCI_ISR_CONFIG"); /* spurious config-change signal */

    return TEST_PASS;
}

REGISTER_TEST(PCI0023, VIRTIO_PCI_DEVICE_BLK, test_pci_isr_config_change,
              "ISR config-change bit not spuriously set on read",
              VIRTIO_SPEC_V1_2, "4.1.4.5");
