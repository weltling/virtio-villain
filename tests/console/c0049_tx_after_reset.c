/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0049: console TX immediately after device reset.
 *
 * Reset the device, reinitialize, then immediately submit a TX
 * buffer. Spec 2.2 says after reset the device must reinitialize
 * status to 0 and not interact with queues until DRIVER_OK. This
 * tests the full re-init path followed by immediate I/O.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_console_tx_after_reset(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    /* Reset */
    virtio_pci_reset(dev);

    /* Reinitialize */
    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    cfg->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();

    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t f0 = cfg->device_feature;
    cfg->driver_feature_select = 0;
    cfg->driver_feature = f0;
    cfg->device_feature_select = 1;
    __sync_synchronize();
    uint32_t f1 = cfg->device_feature;
    cfg->driver_feature_select = 1;
    cfg->driver_feature = f1;
    __sync_synchronize();

    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();
    if (!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK))
        return TEST_SKIP;

    /* Set up TX queue (queue 1 for console) */
    struct vring txq;
    vring_alloc(&txq, 16);
    vring_attach(dev, &txq, 1);

    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    /* Submit TX data immediately */
    uint8_t *buf = vv_alloc_pages(1);
    memcpy(buf, "hello\n", 6);

    vring_raw_set_desc(&txq, 0, vv_virt_to_phys(buf), 6, 0, 0);
    vring_raw_set_avail(&txq, 0, 0);
    vring_raw_set_avail_idx(&txq, 1);

    return vv_kick_and_wait(dev, &txq, 1, VV_TIMEOUT_MS);
}

REGISTER_TEST(C0049, VIRTIO_PCI_DEVICE_CONSOLE,
              test_console_tx_after_reset,
              "TX immediately after device reset and reinit",
              VIRTIO_SPEC_V1_2, "5.3.6");
