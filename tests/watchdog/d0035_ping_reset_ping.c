/* SPDX-License-Identifier: Apache-2.0 */
/*
 * D0035: ping then device reset then ping again.
 *
 * Submit a ping, reset the device, reinitialize, then ping again.
 * Tests that the watchdog correctly restarts its timer state after
 * a full device reset cycle.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_watchdog_reset_ping(struct virtio_dev *dev,
                                              struct vring *vr)
{
    /* First ping */
    uint8_t *buf1 = vv_alloc_pages(1);
    *buf1 = 0;
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(buf1), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;

    /* Reset */
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    virtio_pci_reset(dev);
    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER;
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

    struct vring nv;
    vring_alloc(&nv, 16);
    vring_attach(dev, &nv, 0);
    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    /* Second ping after reset */
    uint8_t *buf2 = vv_alloc_pages(1);
    *buf2 = 0;
    vring_raw_set_desc(&nv, 0, vv_virt_to_phys(buf2), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&nv, 0, 0);
    vring_raw_set_avail_idx(&nv, 1);

    return vv_kick_and_wait(dev, &nv, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(D0035, VIRTIO_PCI_DEVICE_WATCHDOG, test_watchdog_reset_ping,
              "Ping, reset device, reinit, ping again",
              VIRTIO_SPEC_V1_4, "-");
