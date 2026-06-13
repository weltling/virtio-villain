/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0033: Send PORT_OPEN for an invalid port id on the control VQ.
 *
 * Spec 5.3.6.1: VIRTIO_CONSOLE_PORT_OPEN with an out of range
 * port id. The device must reject or ignore gracefully.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_console_ctrl_bad_port(struct virtio_dev *dev,
                                                struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t feat = cfg->device_feature;
    if (!(feat & (1u << VIRTIO_CONSOLE_F_MULTIPORT)))
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
    msg->id = 0xFFFFFFFF; /* invalid port id */
    msg->event = VIRTIO_CONSOLE_PORT_OPEN;
    msg->value = 1;

    vring_raw_set_desc(&cq, 0, vv_virt_to_phys(msg), sizeof(*msg), 0, 0);
    vring_raw_set_avail(&cq, 0, 0);
    vring_raw_set_avail_idx(&cq, 1);

    return vv_kick_and_wait(dev, &cq, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST(C0033, VIRTIO_PCI_DEVICE_CONSOLE,
              test_console_ctrl_bad_port,
              "PORT_OPEN with invalid port id",
              VIRTIO_SPEC_V1_2, "5.3.6.1");
