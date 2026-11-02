/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0018: pci_notify_beyond_num_queues
 *
 * Write to the notification BAR with a queue index larger than
 * num_queues. The device must ignore spurious notifications for
 * non-existent queues without crashing.
 */
#include "lib/virtio_test.h"

static test_result_t test_pci_notify_beyond_queues(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    uint16_t nq = cfg->num_queues;

    /*
     * Write a notification for queue index = num_queues + 10
     * (well beyond valid range). Use the raw notify address.
     */
    if (dev->notify_base) {
        volatile uint16_t *notify = (volatile uint16_t *)(
            (uint8_t *)dev->notify_base +
            dev->notify_off_multiplier * 0);
        /* Write out-of-range queue index */
        *notify = nq + 10;
        __sync_synchronize();
    }

    usleep(50000); /* let device process the bogus notification */

    /* Verify device is still alive by doing a real I/O */
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(PCI0018, VIRTIO_PCI_DEVICE_BLK, test_pci_notify_beyond_queues,
              "Notify with queue index beyond num_queues",
              VIRTIO_SPEC_V1_2, "4.1.4.4");
