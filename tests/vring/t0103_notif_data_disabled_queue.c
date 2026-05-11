/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0103: NOTIFICATION_DATA for a disabled queue.
 *
 * Spec 4.1.5.2: Write an encoded notification for a queue that
 * has not been enabled (queue_enable still 0). The device must
 * ignore the notification and not crash.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

#define VIRTIO_F_NOTIFICATION_DATA_BIT 38

static test_result_t test_notif_data_disabled_q(struct virtio_dev *dev,
                                                struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    cfg->device_feature_select = 1;
    __sync_synchronize();
    uint32_t hi = cfg->device_feature;
    if (!(hi & (1u << (VIRTIO_F_NOTIFICATION_DATA_BIT - 32))))
        return TEST_SKIP;

    virtio_pci_reset(dev);
    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    cfg->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();

    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t f0 = cfg->device_feature;
    cfg->device_feature_select = 1;
    __sync_synchronize();
    uint32_t f1 = cfg->device_feature;

    cfg->driver_feature_select = 0;
    cfg->driver_feature = f0;
    cfg->driver_feature_select = 1;
    cfg->driver_feature = f1;
    __sync_synchronize();

    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();
    if (!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK))
        TREJECT("!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK)");

    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    /* Do NOT enable any queue. Send notification for queue 0. */
    cfg->queue_select = 0;
    __sync_synchronize();
    uint16_t noff = cfg->queue_notify_off;
    volatile uint32_t *addr = (volatile uint32_t *)
        ((char *)dev->notify_base + (uint32_t)noff *
         dev->notify_off_multiplier);
    uint32_t encoded = (uint32_t)0 | ((uint32_t)1 << 16);
    *addr = encoded;
    __sync_synchronize();

    usleep(200000);

    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    TREJECT("no device response within timeout");
}

REGISTER_TEST(T0103, VIRTIO_PCI_DEVICE_BLK, test_notif_data_disabled_q,
              "NOTIFICATION_DATA for queue not enabled",
              VIRTIO_SPEC_V1_2, "4.1.5.2");
