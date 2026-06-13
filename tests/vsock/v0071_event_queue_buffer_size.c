/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0071: event queue accepts a writable buffer.
 *
 * Spec 5.10.5: queue 2 is the event queue. The driver posts a
 * writable buffer of at least sizeof(virtio_vsock_event). The
 * device pushes events into it on transport state changes.
 * Submit a buffer and check the device does not wedge.
 */
#include "tests/test.h"
#include "lib/virtio_spec.h"
#include "lib/util.h"

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    (void)vr;

    cfg->queue_select = 2;
    __sync_synchronize();
    if (cfg->queue_size == 0) return TEST_SKIP;

    struct vring ev;
    if (vring_alloc(&ev, 16) < 0) return TEST_SKIP;
    vring_attach(dev, &ev, 2);

    uint8_t *buf = vv_alloc_pages(1);
    vring_raw_set_desc(&ev, 0, vv_virt_to_phys(buf), 64,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&ev, 0, 0);
    vring_raw_set_avail_idx(&ev, 1);
    virtio_pci_kick(dev, ev.queue);

    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    return TEST_PASS;
}

REGISTER_TEST(V0071, VIRTIO_PCI_DEVICE_VSOCK, test,
              "Event queue accepts a writable buffer",
              VIRTIO_SPEC_V1_4, "5.10.5");
