/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0029: Write device_status=0 (reset) while a request is still in
 * flight (submitted but not yet completed).
 *
 * Spec 2.1.2: The device MUST reset when the driver writes 0 to
 * device_status, regardless of in-flight operations. After reset
 * the device should be re-initializable.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_device_reset_during_io(struct virtio_dev *dev,
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

    /* Kick to start I/O */
    __sync_synchronize();
    virtio_pci_kick(dev, vr->queue);

    /* Immediately reset while request is in flight */
    cfg->device_status = 0;
    __sync_synchronize();

    /* Wait for reset to complete */
    int tries = 200;
    while (tries-- > 0 && cfg->device_status != 0)
        usleep(1000);

    if (cfg->device_status != 0)
        TWEDGED("cfg->device_status != 0");

    /* Re-initialize to verify device is usable again */
    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER;
    __sync_synchronize();

    cfg->driver_feature_select = 0;
    cfg->driver_feature = 0;
    cfg->driver_feature_select = 1;
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

REGISTER_TEST(S0029, VIRTIO_PCI_DEVICE_BLK, test_device_reset_during_io,
              "Device reset (status=0) while I/O is in flight",
              VIRTIO_SPEC_V1_2, "2.1.2");
