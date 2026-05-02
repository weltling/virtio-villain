/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0060: avail_update_before_driver_ok
 *
 * Write a descriptor and update avail.idx (make buffer available) before
 * the device reaches DRIVER_OK state. Unlike T30 which sends a kick,
 * this only modifies the shared memory structures without notification.
 * Then after reaching DRIVER_OK, send the kick.
 *
 * This tests whether a VMM that polls avail.idx (rather than relying
 * solely on kicks) might pick up the buffer prematurely.
 *
 * Spec 2.7.21: driver MUST NOT send any buffer available notifications
 * to the device before setting DRIVER_OK.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} __attribute__((packed));

#define VIRTIO_BLK_T_IN 0

static test_result_t test_avail_update_before_ok(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    (void)vr;

    /* Reset and re-init up to queue setup but NOT DRIVER_OK */
    virtio_pci_reset(dev);

    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    cfg->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();

    cfg->driver_feature_select = 0;
    cfg->driver_feature = 0;
    cfg->driver_feature_select = 1;
    cfg->driver_feature = 0;
    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();

    if (!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK))
        return TEST_SKIP;

    struct vring vr2;
    vring_alloc(&vr2, 16);
    vring_attach(dev, &vr2, 0);

    /* Set up a valid request BEFORE DRIVER_OK */
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    vring_raw_set_desc(&vr2, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&vr2, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(&vr2, 2, vv_virt_to_phys(status), 1,
                       VRING_DESC_F_WRITE, 0);

    /* Make buffer available (update avail.idx) BEFORE DRIVER_OK */
    vring_raw_set_avail(&vr2, 0, 0);
    vring_raw_set_avail_idx(&vr2, 1);
    __sync_synchronize();

    /* Wait a bit - if VMM polls, it might see this prematurely */
    usleep(50000);

    /* NOW set DRIVER_OK and kick */
    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    return vv_kick_and_wait(dev, &vr2, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0060, VIRTIO_PCI_DEVICE_BLK, test_avail_update_before_ok,
              "Update avail.idx before DRIVER_OK then kick after",
              VIRTIO_SPEC_V1_2, "2.7.21");
