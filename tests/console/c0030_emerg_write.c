/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0030: Write to the emergency write register (config offset 12).
 *
 * Spec 5.3.4: When VIRTIO_CONSOLE_F_EMERG_WRITE is negotiated,
 * a driver may write a single byte to emerg_wr (offset 12 in the
 * device specific config). The device should output it immediately.
 * If the feature is not advertised the write should be harmless.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

#define VIRTIO_CONSOLE_F_EMERG_WRITE 2

static test_result_t test_console_emerg_write(struct virtio_dev *dev,
                                              struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    if (dev->device_cfg == NULL)
        return TEST_SKIP;

    /* Check if feature is advertised */
    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t feat = cfg->device_feature;
    if (!(feat & (1u << VIRTIO_CONSOLE_F_EMERG_WRITE)))
        return TEST_SKIP;

    /* Write a byte to emerg_wr at device config offset 12 */
    volatile uint32_t *emerg = (volatile uint32_t *)
        ((char *)dev->device_cfg + 12);
    *emerg = (uint32_t)'!';
    __sync_synchronize();

    /* Write again */
    *emerg = (uint32_t)'\n';
    __sync_synchronize();

    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    return TEST_PASS;
}

REGISTER_TEST(C0030, VIRTIO_PCI_DEVICE_CONSOLE, test_console_emerg_write,
              "Emergency write register output",
              VIRTIO_SPEC_V1_3, "5.3.4");
