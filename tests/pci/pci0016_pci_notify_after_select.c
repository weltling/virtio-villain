/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0016: pci_notify_wrong_queue_after_select
 *
 * Write to the notification BAR with the wrong queue index after
 * changing queue_select. This tests whether the device's notify
 * dispatch correctly identifies the target queue from the BAR
 * offset rather than the queue_select register.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_notify_wrong_queue_after_select(struct virtio_dev *dev,
                                                         struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);
    uint64_t status_phys = vv_virt_to_phys(status);

    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, data_phys, 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    __sync_synchronize();

    /*
     * Change queue_select to point at queue 3 (or highest available)
     * then kick queue 0 via the notify BAR offset for queue 0.
     * The device should still process queue 0 correctly since the
     * notify offset encodes the queue, not queue_select.
     */
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    cfg->queue_select = 3;
    __sync_synchronize();

    /* Kick queue 0 using the correct notify offset */
    virtio_pci_kick(dev, 0);

    int elapsed = 0;
    int step = 10000;
    while (elapsed < VV_TIMEOUT_MS * 1000) {
        usleep(step);
        __sync_synchronize();
        if (vr->used->idx != 0)
            return TEST_PASS;
        elapsed += step;
    }

    volatile struct virtio_pci_common_cfg *cfg2 = dev->common;
    __sync_synchronize();
    uint8_t dev_status = cfg2->device_status;
    if (dev_status == 0)
        TWEDGED("dev_status == 0");
    TREJECT("no device response within timeout");
}

REGISTER_TEST(PCI0016, VIRTIO_PCI_DEVICE_BLK, test_notify_wrong_queue_after_select,
              "Notify queue 0 after queue_select changed to 3",
              VIRTIO_SPEC_V1_2, "4.1.4.4");
