/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0094: device must not consume buffers before DRIVER_OK.
 *
 * Spec 2.1.2: Until DRIVER_OK is set, the device MUST NOT
 * complete any virtqueue requests. Reset, take init only to
 * FEATURES_OK (skip DRIVER_OK), program the queue, post a
 * descriptor and kick. The used ring must not advance.
 */
#include "tests/test.h"
#include "lib/util.h"

#include <string.h>
#include <unistd.h>

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    virtio_pci_reset(dev);
    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER;
    __sync_synchronize();
    cfg->driver_feature_select = 0; cfg->driver_feature = 0;
    cfg->driver_feature_select = 1; cfg->driver_feature = 0;
    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();
    usleep(1000);
    if (!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK))
        TREJECT("FEATURES_OK rejected");

    struct vring tvr;
    if (vring_alloc(&tvr, 16) < 0) return TEST_SKIP;
    vring_attach(dev, &tvr, 0);
    /* DRIVER_OK intentionally not set. */

    uint8_t *buf = vv_alloc_pages(1);
    vring_raw_set_desc(&tvr, 0, vv_virt_to_phys(buf), 64,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&tvr, 0, 0);
    vring_raw_set_avail_idx(&tvr, 1);

    test_result_t r = vv_kick_expect_reject(dev, &tvr, VV_TIMEOUT_MS);
    /* Now bring up DRIVER_OK for a clean exit. */
    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();
    return r;
}

REGISTER_TEST(S0094, VIRTIO_PCI_DEVICE_BLK, test,
              "Device must not consume buffers before DRIVER_OK",
              VIRTIO_SPEC_V1_4, "2.1.2");
