/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0013: queue_setup_after_driver_ok
 *
 * After reaching DRIVER_OK and using queue 0, attempt to configure and
 * enable a second queue (queue index 1) without resetting or going back
 * through the initialization sequence. The spec says queue configuration
 * must happen before DRIVER_OK (3.1.1 step 7: "perform device-specific
 * setup, including... virtqueue configuration").
 *
 * A VMM that allows late queue setup may have uninitialized state for
 * the newly "configured" queue, or may not properly synchronize with
 * the already-running device.
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

static test_result_t test_queue_setup_after_driver_ok(struct virtio_dev *dev,
                                                      struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* Check if device has more than one queue */
    uint16_t num_queues = cfg->num_queues;
    if (num_queues < 2) {
        /* Blk devices typically have 1 queue - try anyway */
    }

    /*
     * Device is in DRIVER_OK. Now try to configure queue 1 live.
     * This is the violation - queues should be set up before DRIVER_OK.
     */
    struct vring vr2;
    vring_alloc(&vr2, 16);

    cfg->queue_select = 1;
    __sync_synchronize();

    uint16_t q1_size = cfg->queue_size;
    if (q1_size == 0) {
        /* Queue doesn't exist - that's fine, try writing config anyway */
        cfg->queue_size = 16;
    }

    /* Write queue addresses for queue 1 while in DRIVER_OK */
    cfg->queue_desc = vr2.desc_phys;
    cfg->queue_avail = vr2.avail_phys;
    cfg->queue_used = vr2.used_phys;
    cfg->queue_msix_vector = 0xffff;
    cfg->queue_enable = 1;
    __sync_synchronize();
    usleep(10000);

    /*
     * Try to use queue 1. For blk devices this queue likely doesn't
     * have defined semantics, but the point is whether the VMM crashes
     * from late queue enablement.
     */
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

    /* Kick queue 1 */
    virtio_pci_kick(dev, 1);
    usleep(200000);
    __sync_synchronize();

    /*
     * Also verify queue 0 still works after this abuse.
     */
    struct virtio_blk_outhdr *hdr0 = vv_alloc_pages(1);
    uint8_t *data0 = vv_alloc_pages(1);
    uint8_t *status0 = vv_alloc_pages(1);

    hdr0->type = VIRTIO_BLK_T_IN;
    hdr0->ioprio = 0;
    hdr0->sector = 0;
    *status0 = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr0), sizeof(*hdr0),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data0), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(status0), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(S0013, VIRTIO_PCI_DEVICE_BLK, test_queue_setup_after_driver_ok,
              "Configure and enable a new queue after DRIVER_OK",
              VIRTIO_SPEC_V1_2, "3.1.1");
