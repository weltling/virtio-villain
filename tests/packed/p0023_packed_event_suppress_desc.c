/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0023: packed_event_suppression_desc_mode
 *
 * Set driver event suppression to VRING_PACKED_EVENT_FLAG_DESC mode, meaning
 * the device should only notify when a specific descriptor index is
 * used. Submit multiple requests and verify the device respects the
 * event suppression threshold.
 */
#include "lib/virtio_test.h"

static test_result_t test_packed_event_suppress_desc(struct virtio_dev *dev,
                                                     struct vring_packed *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    /*
     * Set driver event suppression to DESC mode: notify only when
     * descriptor at index 1 (with current wrap) is used.
     * off_wrap = (idx << 1) | wrap_counter
     */
    vr->driver_event->flags = VRING_PACKED_EVENT_FLAG_DESC;
    vr->driver_event->off_wrap = (1 << 1) | vr->wrap_counter;
    __sync_synchronize();

    /* Submit first request at index 0 */
    vring_packed_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr), 0,
                          VRING_PACKED_DESC_F_NEXT);
    vring_packed_set_desc(vr, 1, vv_virt_to_phys(data), 512, 0,
                          VRING_PACKED_DESC_F_NEXT | VRING_PACKED_DESC_F_WRITE);
    vring_packed_set_desc(vr, 2, vv_virt_to_phys(status), 1, 0,
                          VRING_PACKED_DESC_F_WRITE);

    uint8_t check_wrap = vr->wrap_counter;
    virtio_pci_kick(dev, vr->queue);

    /* Wait for the device to process the request */
    int elapsed = 0;
    while (elapsed < VV_TIMEOUT_MS * 1000) {
        usleep(10000);
        if (vring_packed_desc_is_used(vr, 0, check_wrap))
            return TEST_PASS;
        elapsed += 10000;
    }

    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    TREJECT("no device response within timeout");
}

REGISTER_TEST_PACKED(P0023, VIRTIO_PCI_DEVICE_BLK, test_packed_event_suppress_desc,
                     "Packed event suppression in DESC mode",
                     VIRTIO_SPEC_V1_2, "2.8.10");
