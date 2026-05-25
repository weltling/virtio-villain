/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0039: Driver sends PORT_READY for a port that was never
 * announced via DEVICE_ADD.
 *
 * Spec 5.3.4: PORT_READY (event 3) acknowledges a prior DEVICE_ADD
 * from the device. Send PORT_READY for a high port_id without
 * having received DEVICE_ADD for it. The device must reject the
 * stray ack without setting up new port state.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

#define VIRTIO_CONSOLE_F_MULTIPORT  1
#define VIRTIO_CONSOLE_PORT_READY   3

struct virtio_console_control {
    uint32_t id;
    uint16_t event;
    uint16_t value;
} __attribute__((packed));

static test_result_t test_console_port_ready_unannounced(struct virtio_dev *dev,
                                                         struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1u << VIRTIO_CONSOLE_F_MULTIPORT)))
        return TEST_SKIP;

    cfg->queue_select = 2;
    __sync_synchronize();
    if (cfg->queue_size == 0)
        return TEST_SKIP;

    struct vring cq;
    vring_alloc(&cq, 16);
    vring_attach(dev, &cq, 2);

    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    struct virtio_console_control *msg = vv_alloc_pages(1);
    msg->id    = 7;    /* never received DEVICE_ADD for this id */
    msg->event = VIRTIO_CONSOLE_PORT_READY;
    msg->value = 1;

    vring_raw_set_desc(&cq, 0, vv_virt_to_phys(msg), sizeof(*msg), 0, 0);
    vring_raw_set_avail(&cq, 0, 0);
    vring_raw_set_avail_idx(&cq, 1);

    return vv_kick_and_wait(dev, &cq, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST(C0039, VIRTIO_PCI_DEVICE_CONSOLE,
              test_console_port_ready_unannounced,
              "PORT_READY for a port that was never announced",
              VIRTIO_SPEC_V1_2, "5.3.4");
