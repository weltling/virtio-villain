/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0123: reset_during_inflight
 *
 * Kick a read request and then immediately reset the device by writing
 * zero to device_status while that request may still be in flight on the
 * host. The device reset must drain or cancel the in flight request
 * cleanly rather than complete it into a torn down queue or deadlock the
 * drain. A hang or a sanitizer fault is a guest triggered host problem.
 * The device outcome is up to it as long as the host survives and the
 * next test can still run.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_reset_during_inflight(struct virtio_dev *dev,
                                                struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    memset(data, 0, 512);
    *status = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(status), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* Notify, then reset immediately to race the in flight request. */
    virtio_pci_kick(dev, vr->queue);
    cfg->device_status = 0;
    __sync_synchronize();

    /* Liveness probe: repeatedly touch the device MMIO. If the host is
     * wedged on the reset drain, these reads block and the runner times
     * out. */
    for (int i = 0; i < 1000; i++) {
        (void)cfg->device_status;
        __sync_synchronize();
    }

    return TEST_PASS;
}

REGISTER_TEST(T0123, VIRTIO_PCI_DEVICE_BLK, test_reset_during_inflight,
              "Reset the device while a request may be in flight",
              VIRTIO_SPEC_V1_2, "2.4.1");
