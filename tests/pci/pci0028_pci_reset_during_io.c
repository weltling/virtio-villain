/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0028: PCI FLR (function-level reset) during I/O (spec 4.1.4.3.2)
 *
 * Submit a block request, then immediately perform a device reset
 * via the PCI status register. This tests the device's ability to
 * cleanly tear down in-flight I/O. We use virtio status=0 as the
 * closest userspace equivalent to FLR.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_pci_reset_during_io(struct virtio_dev *dev,
                                              struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* Submit a request */
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *st = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    /* Kick then immediately reset */
    __sync_synchronize();
    virtio_pci_kick(dev, 0);

    /* Immediate reset (FLR equivalent) */
    cfg->device_status = 0;
    __sync_synchronize();

    /* Wait for reset to complete */
    int tries = 100;
    while (tries-- > 0 && cfg->device_status != 0)
        usleep(1000);

    if (cfg->device_status != 0)
        TWEDGED("cfg->device_status != 0");

    /* Re-init the device to verify it recovered */
    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER;
    __sync_synchronize();

    cfg->driver_feature_select = 0;
    cfg->driver_feature = 0;
    __sync_synchronize();

    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();
    usleep(5000);

    if (!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK))
        TREJECT("!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK)");

    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    return TEST_PASS;
}

REGISTER_TEST(PCI0028, VIRTIO_PCI_DEVICE_BLK, test_pci_reset_during_io,
              "Device reset (FLR equivalent) during in-flight I/O",
              VIRTIO_SPEC_V1_2, "4.1.4.3.2");
