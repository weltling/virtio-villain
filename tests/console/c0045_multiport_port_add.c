/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0045: MULTIPORT PORT_ADD path is reachable by driver.
 *
 * Spec 5.3.6.1: PORT_ADD is sent by the device; the driver
 * responds with PORT_READY. We exercise the receive side by
 * posting a writable buffer on the control receive queue
 * (idx 3) so the device can deliver a PORT_ADD message.
 */
#include "tests/test.h"
#include "lib/virtio_spec.h"
#include "lib/util.h"

#include <string.h>

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    (void)vr;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_CONSOLE_F_MULTIPORT)))
        return TEST_SKIP;

    cfg->queue_select = 3;
    __sync_synchronize();
    if (cfg->queue_size == 0) return TEST_SKIP;

    struct vring rxctl;
    if (vring_alloc(&rxctl, 16) < 0) return TEST_SKIP;
    vring_attach(dev, &rxctl, 3);

    uint8_t *buf = vv_alloc_pages(1);
    vring_raw_set_desc(&rxctl, 0, vv_virt_to_phys(buf), 64,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&rxctl, 0, 0);
    vring_raw_set_avail_idx(&rxctl, 1);
    virtio_pci_kick(dev, rxctl.queue);

    /* Device may or may not push PORT_ADD here; alive check only. */
    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(C0045, VIRTIO_PCI_DEVICE_CONSOLE, test,
              "MULTIPORT PORT_ADD receive buffer posted",
              VIRTIO_SPEC_V1_4, "5.3.6.1",
              (1ULL << VIRTIO_CONSOLE_F_MULTIPORT), 0);
