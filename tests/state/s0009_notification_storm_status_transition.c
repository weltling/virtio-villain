/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0009: notification_storm_status_transition
 *
 * Send a burst of queue notifications (kicks) while simultaneously
 * transitioning device_status through multiple states. The spec says
 * notifications MUST NOT be sent before DRIVER_OK (2.7.21), but here
 * we test the VMM's resilience to a rapid-fire notification storm
 * interleaved with status register writes.
 *
 * This exercises locking and synchronization in the VMM's notification
 * handling path - a race between the MMIO/PIO write handler for kicks
 * and the status transition logic.
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

static test_result_t test_notification_storm(struct virtio_dev *dev,
                                             struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    (void)vr;

    /* Device is in DRIVER_OK. Start the storm. */

    /*
     * Phase 1: Rapid kicks while device is live.
     * No descriptors are actually pending - just naked notifications.
     */
    for (int i = 0; i < 50; i++)
        virtio_pci_kick(dev, 0);

    /*
     * Phase 2: Interleave kicks with status transitions.
     * Write FAILED while kicks are still being issued.
     */
    virtio_pci_kick(dev, 0);
    cfg->device_status |= VIRTIO_STATUS_FAILED;
    __sync_synchronize();
    virtio_pci_kick(dev, 0);
    virtio_pci_kick(dev, 0);

    usleep(10000);

    /*
     * Phase 3: Reset device while issuing kicks.
     */
    cfg->device_status = 0;
    virtio_pci_kick(dev, 0);
    __sync_synchronize();
    virtio_pci_kick(dev, 0);
    virtio_pci_kick(dev, 0);

    usleep(50000);

    /*
     * Phase 4: Re-init, but kick before DRIVER_OK.
     */
    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    virtio_pci_kick(dev, 0);
    __sync_synchronize();
    cfg->device_status |= VIRTIO_STATUS_DRIVER;
    virtio_pci_kick(dev, 0);
    __sync_synchronize();

    cfg->driver_feature_select = 0;
    cfg->driver_feature = 0;
    cfg->driver_feature_select = 1;
    cfg->driver_feature = 0;
    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();

    if (!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK))
        return TEST_SKIP;

    virtio_pci_kick(dev, 0); /* kick before queue setup */

    struct vring vr2;
    vring_alloc(&vr2, 16);
    vring_attach(dev, &vr2, 0);

    virtio_pci_kick(dev, 0); /* kick before DRIVER_OK */

    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    /* Now verify device works normally */
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
    vring_raw_set_avail(&vr2, 0, 0);
    vring_raw_set_avail_idx(&vr2, 1);

    return vv_kick_and_wait(dev, &vr2, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(S0009, VIRTIO_PCI_DEVICE_BLK, test_notification_storm,
              "Burst notifications interleaved with status transitions",
              VIRTIO_SPEC_V1_2, "2.7.21");
