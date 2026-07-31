/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0236: hotplug_remove_event
 *
 * Post buffers on the event queue while a host side sidecar hot adds a
 * logical unit and then removes it. The guest confirms a transport
 * reset event arrives carrying the removed reason.
 *
 * Runs under QEMU with the API socket enabled. When no event arrives
 * the test skips.
 */
#include "tests/scsi/scsi_util.h"

#include <unistd.h>

#define NBUF 4

static test_result_t test_scsi_unplug_event(struct virtio_dev *dev,
                                            struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_SCSI_F_HOTPLUG))
        TSKIP("hot plug feature not offered");

    struct virtio_scsi_event *ev[NBUF];
    uint16_t before = vr->used->idx;
    for (int i = 0; i < NBUF; i++) {
        ev[i] = vv_alloc_pages(1);
        memset(ev[i], 0, sizeof(*ev[i]));
        vring_raw_set_desc(vr, i, vv_virt_to_phys(ev[i]), sizeof(*ev[i]),
                           VRING_DESC_F_WRITE, 0);
        vring_raw_set_avail(vr, i, (uint16_t)i);
    }
    vring_raw_set_avail_idx(vr, NBUF);
    virtio_pci_kick(dev, vr->queue);

    printf("vv-scsi-armed\n");
    fflush(stdout);

    /* Wait for the add and remove events, allowing time for the host. */
    int elapsed = 0;
    while (elapsed < 15000000) {
        usleep(20000);
        __sync_synchronize();
        if ((uint16_t)(vr->used->idx - before) >= 2)
            break;
        elapsed += 20000;
    }

    __sync_synchronize();
    if (vr->used->idx == before)
        TSKIP("no event delivered (no host hot add)");

    for (int i = 0; i < NBUF; i++) {
        uint32_t code = ev[i]->event & ~VIRTIO_SCSI_T_EVENTS_MISSED;
        if (code == VIRTIO_SCSI_T_TRANSPORT_RESET &&
            ev[i]->reason == VIRTIO_SCSI_EVT_RESET_REMOVED)
            return TEST_PASS;
    }
    TFAIL("no transport reset with removed reason among events");
}

REGISTER_TEST_Q_REQUIRES(SCSI0236, VIRTIO_PCI_DEVICE_SCSI,
                         test_scsi_unplug_event,
                         "Hot removing a logical unit delivers a removed event",
                         VIRTIO_SPEC_V1_4, "5.6.6.3", 1,
                         VV_FEATURE_BIT(VIRTIO_SCSI_F_HOTPLUG), 0);
