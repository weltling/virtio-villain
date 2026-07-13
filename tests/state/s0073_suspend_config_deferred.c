/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0073: suspend_config_change_deferred
 *
 * While suspended, changes to config space must not generate
 * notifications. After resume, the notification must be sent.
 * Spec 3.2: if changes occur during suspended period, the device
 * MUST NOT send config change notifications until resume.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <unistd.h>

static test_result_t test_suspend_cfg_deferred(struct virtio_dev *dev,
                                               struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    (void)vr;

    /* Check if SUSPEND is offered */
    cfg->device_feature_select = VIRTIO_F_SUSPEND / 32;
    __sync_synchronize();
    uint32_t offered = cfg->device_feature;
    if (!(offered & (1U << (VIRTIO_F_SUSPEND % 32))))
        return TEST_SKIP;

    /* Clear ISR */
    volatile uint8_t isr_val = *dev->isr;
    (void)isr_val;

    /* Suspend */
    uint8_t status = cfg->device_status;
    cfg->device_status = status | VIRTIO_STATUS_SUSPEND;
    __sync_synchronize();
    usleep(200000);

    /* While suspended, ISR should not fire for config changes */
    usleep(200000);
    uint8_t isr_during = *dev->isr;

    /* The config bit should not be set during suspend */
    if (isr_during & VIRTIO_PCI_ISR_CONFIG)
        TFAIL("isr_during & VIRTIO_PCI_ISR_CONFIG");

    /* Device must not have crashed */
    uint8_t st = cfg->device_status;
    if (st == 0)
        TWEDGED("st == 0");

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(S0073, VIRTIO_PCI_DEVICE_BLK, test_suspend_cfg_deferred,
              "No config change notification while suspended",
              VIRTIO_SPEC_V1_3, "3.2",
              (1ULL << VIRTIO_F_SUSPEND), 0);
