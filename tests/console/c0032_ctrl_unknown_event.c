/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0032: Send unknown control message type on the control VQ.
 *
 * Spec 5.3.6.1: When VIRTIO_CONSOLE_F_MULTIPORT is negotiated,
 * the control VQ (queue 2) carries control messages. Sending a
 * message with an undefined event type must not crash the device.
 * If MULTIPORT is not offered, skip.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

#define VIRTIO_CONSOLE_F_MULTIPORT 1

struct virtio_console_control {
    uint32_t id;
    uint16_t event;
    uint16_t value;
} __attribute__((packed));

static test_result_t test_console_ctrl_unknown(struct virtio_dev *dev,
                                               struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t feat = cfg->device_feature;
    if (!(feat & (1u << VIRTIO_CONSOLE_F_MULTIPORT)))
        return TEST_SKIP;

    /* Queue 2 is the TX ctrl queue */
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
    msg->id = 0;
    msg->event = 0xFFFF; /* unknown event */
    msg->value = 1;

    vring_raw_set_desc(&cq, 0, vv_virt_to_phys(msg), sizeof(*msg), 0, 0);
    vring_raw_set_avail(&cq, 0, 0);
    vring_raw_set_avail_idx(&cq, 1);

    return vv_kick_and_wait(dev, &cq, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST(C0032, VIRTIO_PCI_DEVICE_CONSOLE,
              test_console_ctrl_unknown,
              "Unknown control message type on ctrl VQ",
              VIRTIO_SPEC_V1_2, "5.3.6.1");
