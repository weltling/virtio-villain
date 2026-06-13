/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0040: Driver sends PORT_NAME event on the control queue.
 *
 * Spec 5.3.4: PORT_NAME (event 7) is device-to-driver only. The
 * driver MUST NOT originate PORT_NAME messages. Send one from
 * the driver side; the device must drop the message without
 * updating any port metadata or echoing the unsolicited name.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_console_driver_sends_port_name(struct virtio_dev *dev,
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

    uint8_t *p = vv_alloc_pages(1);
    memset(p, 0, 4096);
    struct virtio_console_control *msg = (struct virtio_console_control *)p;
    msg->id    = 0;
    msg->event = VIRTIO_CONSOLE_PORT_NAME;
    msg->value = 0;
    memcpy(p + sizeof(*msg), "evil\0", 5);

    vring_raw_set_desc(&cq, 0, vv_virt_to_phys(p),
                       sizeof(*msg) + 5, 0, 0);
    vring_raw_set_avail(&cq, 0, 0);
    vring_raw_set_avail_idx(&cq, 1);

    return vv_kick_and_wait(dev, &cq, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST(C0040, VIRTIO_PCI_DEVICE_CONSOLE,
              test_console_driver_sends_port_name,
              "Driver originates PORT_NAME on the control queue",
              VIRTIO_SPEC_V1_2, "5.3.4");
