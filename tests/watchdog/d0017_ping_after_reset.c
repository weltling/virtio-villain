/* SPDX-License-Identifier: Apache-2.0 */
/*
 * D0017: Ping after device reset without reinitializing.
 *
 * Spec 2.4: After setting device_status to 0, the driver must
 * not use the device without full reinitialization. Submit a
 * descriptor to the queue after reset. The device must not crash.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_watchdog_ping_after_reset(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* Reset the device */
    cfg->device_status = 0;
    __sync_synchronize();

    int tries = 500;
    while (tries-- > 0 && cfg->device_status != 0)
        usleep(1000);
    if (cfg->device_status != 0)
        TWEDGED("cfg->device_status != 0");

    /* Try to kick the old queue without reinit */
    uint8_t *buf = vv_alloc_pages(1);
    *buf = 0;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(buf), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    __sync_synchronize();
    virtio_pci_kick(dev, vr->queue);
    usleep(500000);

    /* Device must not have crashed from the stale kick */
    TREJECT("no device response within timeout");
}

REGISTER_TEST(D0017, VIRTIO_PCI_DEVICE_WATCHDOG, test_watchdog_ping_after_reset,
              "Kick queue after reset without reinit",
              VIRTIO_SPEC_V1_2, "2.4");
