/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0235: hotplug_event
 *
 * Post a buffer on the event queue and wait for a transport reset
 * event. A host side sidecar hot adds a logical unit, which makes the
 * device deliver a rescan event. The guest confirms the event carries
 * the transport reset code.
 *
 * Runs under QEMU with the API socket enabled. When no event arrives,
 * for example on a backend without the host action, the test skips.
 */
#include "tests/scsi/scsi_util.h"

#include <unistd.h>

static test_result_t test_scsi_hotplug_event(struct virtio_dev *dev,
                                             struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_SCSI_F_HOTPLUG))
        TSKIP("hot plug feature not offered");

    struct virtio_scsi_event *ev = vv_alloc_pages(1);
    memset(ev, 0, sizeof(*ev));

    uint16_t before = vr->used->idx;
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(ev), sizeof(*ev),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    virtio_pci_kick(dev, vr->queue);

    /* Tell the sidecar the event queue is armed so it can hot add. */
    printf("vv-scsi-armed\n");
    fflush(stdout);

    /* Wait for the device to deliver an event, allowing time for the
     * host side hot add. */
    int elapsed = 0;
    while (elapsed < 15000000) {
        usleep(20000);
        __sync_synchronize();
        if (vr->used->idx != before)
            break;
        elapsed += 20000;
    }

    __sync_synchronize();
    if (vr->used->idx == before)
        TSKIP("no event delivered (no host hot add)");

    uint32_t code = ev->event & ~VIRTIO_SCSI_T_EVENTS_MISSED;
    if (code != VIRTIO_SCSI_T_TRANSPORT_RESET)
        TFAIL("event code %u, expected transport reset", code);
    return TEST_PASS;
}

REGISTER_TEST_Q_REQUIRES(SCSI0235, VIRTIO_PCI_DEVICE_SCSI,
                         test_scsi_hotplug_event,
                         "Hot adding a logical unit delivers a reset event",
                         VIRTIO_SPEC_V1_4, "5.6.6.3", 1,
                         VV_FEATURE_BIT(VIRTIO_SCSI_F_HOTPLUG), 0);
