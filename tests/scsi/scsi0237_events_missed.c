/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0237: events_missed
 *
 * Leave the event queue empty while a host side sidecar hot adds a
 * logical unit, so the device has to drop the event. When the guest
 * then posts a buffer, the device reports an event with the events
 * missed flag set.
 *
 * Runs under QEMU with the API socket enabled. When no event arrives
 * the test skips.
 */
#include "tests/scsi/scsi_util.h"

#include <unistd.h>

static test_result_t test_scsi_events_missed(struct virtio_dev *dev,
                                             struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_SCSI_F_HOTPLUG))
        TSKIP("hot plug feature not offered");

    struct virtio_scsi_event *ev = vv_alloc_pages(1);
    memset(ev, 0, sizeof(*ev));
    uint16_t before = vr->used->idx;

    /* Arm with no buffer posted so the hot add event is dropped. */
    printf("vv-scsi-armed\n");
    fflush(stdout);
    usleep(3000000);

    /* Now offer a buffer; the device reports the missed event. */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(ev), sizeof(*ev),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    virtio_pci_kick(dev, vr->queue);

    int elapsed = 0;
    while (elapsed < 10000000) {
        usleep(20000);
        __sync_synchronize();
        if (vr->used->idx != before)
            break;
        elapsed += 20000;
    }

    __sync_synchronize();
    if (vr->used->idx == before)
        TSKIP("no event delivered (no host hot add)");
    if (!(ev->event & VIRTIO_SCSI_T_EVENTS_MISSED))
        TFAIL("event 0x%08x lacks the events missed flag", ev->event);
    return TEST_PASS;
}

REGISTER_TEST_Q_REQUIRES(SCSI0237, VIRTIO_PCI_DEVICE_SCSI,
                         test_scsi_events_missed,
                         "A dropped event sets the events missed flag",
                         VIRTIO_SPEC_V1_4, "5.6.6.3", 1,
                         VV_FEATURE_BIT(VIRTIO_SCSI_F_HOTPLUG), 0);
