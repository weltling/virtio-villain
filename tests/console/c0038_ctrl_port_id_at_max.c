/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0038: PORT_OPEN with port_id == max_nr_ports.
 *
 * Spec 5.3.4: max_nr_ports is the highest valid port id plus one.
 * A PORT_OPEN targeting exactly max_nr_ports is off by one and
 * must be rejected without crashing the device.
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

static test_result_t test_console_ctrl_port_at_max(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t feat = cfg->device_feature;
    if (!(feat & (1u << VIRTIO_CONSOLE_F_MULTIPORT)))
        return TEST_SKIP;

    /* Read max_nr_ports from device cfg (cols:2, rows:2, max_nr_ports:4) */
    volatile uint8_t *dc = (volatile uint8_t *)dev->device_cfg;
    uint32_t max_nr_ports = *(volatile uint32_t *)(dc + 4);
    if (max_nr_ports == 0 || max_nr_ports == 0xFFFFFFFFU)
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
    msg->id    = max_nr_ports;   /* off by one: valid ids are 0..max-1 */
    msg->event = VIRTIO_CONSOLE_PORT_OPEN;
    msg->value = 1;

    vring_raw_set_desc(&cq, 0, vv_virt_to_phys(msg), sizeof(*msg), 0, 0);
    vring_raw_set_avail(&cq, 0, 0);
    vring_raw_set_avail_idx(&cq, 1);

    return vv_kick_and_wait(dev, &cq, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST(C0038, VIRTIO_PCI_DEVICE_CONSOLE,
              test_console_ctrl_port_at_max,
              "PORT_OPEN with port_id equal to max_nr_ports",
              VIRTIO_SPEC_V1_2, "5.3.4");
