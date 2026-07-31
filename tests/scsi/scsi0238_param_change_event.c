/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0238: param_change_event
 *
 * Post buffers on the event queue while a host side sidecar resizes the
 * backing disk. The capacity change makes the device report a parameter
 * change event.
 *
 * Runs under QEMU with the API socket enabled. When no event arrives
 * the test skips.
 */
#include "tests/scsi/scsi_util.h"

#include <unistd.h>

#define NBUF 2

static test_result_t test_scsi_param_change(struct virtio_dev *dev,
                                            struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_SCSI_F_CHANGE))
        TSKIP("reported change feature not offered");

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
        TSKIP("no event delivered (no host resize)");

    for (int i = 0; i < NBUF; i++) {
        uint32_t code = ev[i]->event & ~VIRTIO_SCSI_T_EVENTS_MISSED;
        if (code == VIRTIO_SCSI_T_PARAM_CHANGE)
            return TEST_PASS;
    }
    TFAIL("no parameter change event among delivered events");
}

REGISTER_TEST_Q_REQUIRES(SCSI0238, VIRTIO_PCI_DEVICE_SCSI,
                         test_scsi_param_change,
                         "A disk resize delivers a parameter change event",
                         VIRTIO_SPEC_V1_4, "5.6.6.3", 1,
                         VV_FEATURE_BIT(VIRTIO_SCSI_F_CHANGE), 0);
