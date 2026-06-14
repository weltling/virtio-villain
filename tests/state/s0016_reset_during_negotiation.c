/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0016: state_reset_during_negotiation
 *
 * Reset the device in the middle of feature negotiation (after DRIVER
 * but before FEATURES_OK). Tests that the device correctly abandons
 * the partial negotiation and returns to a clean initial state.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_reset_during_negotiation(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* Reset to clean state */
    virtio_pci_reset(dev);

    /* Begin negotiation */
    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    cfg->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();

    /* Write some driver features */
    cfg->driver_feature_select = 0;
    cfg->driver_feature = 0xFFFFFFFF; /* garbage features */
    __sync_synchronize();

    /* Reset BEFORE writing FEATURES_OK */
    virtio_pci_reset(dev);

    /* Verify device is back to initial state (status should read 0) */
    if (cfg->device_status != 0)
        TFAIL("cfg->device_status != 0");

    /* Now do a proper full init and I/O */
    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    cfg->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();

    cfg->device_feature_select = 0;
    __sync_synchronize();
    cfg->driver_feature_select = 0;
    cfg->driver_feature = cfg->device_feature;
    __sync_synchronize();

    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();

    if (!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK))
        return TEST_SKIP;

    struct vring q0;
    vring_alloc(&q0, 64);
    vring_attach(dev, &q0, 0);

    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    (void)vr;

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    vring_raw_set_desc(&q0, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&q0, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(&q0, 2, vv_virt_to_phys(status), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(&q0, 0, 0);
    vring_raw_set_avail_idx(&q0, 1);

    return vv_kick_and_wait(dev, &q0, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(S0016, VIRTIO_PCI_DEVICE_BLK, test_reset_during_negotiation,
              "Reset during feature negotiation (before FEATURES_OK)",
              VIRTIO_SPEC_V1_2, "3.1.1");
