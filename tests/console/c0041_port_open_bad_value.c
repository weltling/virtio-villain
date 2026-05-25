/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0041: Console PORT_OPEN with a reserved value field.
 *
 * Spec 5.3.4: The PORT_OPEN control event uses the value field
 * as a boolean (0 = closed, 1 = open). Submit a PORT_OPEN
 * carrying value=0xFFFF. The device must treat non zero as
 * "open" or reject the message; it must not crash or interpret
 * the high bits as an internal state field.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

#define VIRTIO_CONSOLE_F_MULTIPORT 1
#define VIRTIO_CONSOLE_PORT_OPEN   6

struct virtio_console_control {
    uint32_t id;
    uint16_t event;
    uint16_t value;
} __attribute__((packed));

static test_result_t test_console_port_open_bad_value(struct virtio_dev *dev,
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
    msg->id    = 0;
    msg->event = VIRTIO_CONSOLE_PORT_OPEN;
    msg->value = 0xFFFF;

    vring_raw_set_desc(&cq, 0, vv_virt_to_phys(msg), sizeof(*msg), 0, 0);
    vring_raw_set_avail(&cq, 0, 0);
    vring_raw_set_avail_idx(&cq, 1);

    return vv_kick_and_wait(dev, &cq, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST(C0041, VIRTIO_PCI_DEVICE_CONSOLE,
              test_console_port_open_bad_value,
              "PORT_OPEN with value field set to 0xFFFF",
              VIRTIO_SPEC_V1_2, "5.3.4");
