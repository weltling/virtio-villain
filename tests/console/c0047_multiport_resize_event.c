/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0047: MULTIPORT RESIZE event posted on control queue.
 *
 * Spec 5.3.6.1: the driver informs the device of a console
 * window resize via RESIZE with a payload struct giving rows
 * and cols. Submit a resize for port 0.
 */
#include "tests/test.h"
#include "lib/util.h"

#include <string.h>

#define VIRTIO_CONSOLE_F_MULTIPORT 1
#define VIRTIO_CONSOLE_RESIZE 5

struct virtio_console_control {
    uint32_t id;
    uint16_t event;
    uint16_t value;
} __attribute__((packed));

struct virtio_console_resize {
    uint16_t rows;
    uint16_t cols;
} __attribute__((packed));

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    (void)vr;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_CONSOLE_F_MULTIPORT)))
        return TEST_SKIP;

    cfg->queue_select = 2;
    __sync_synchronize();
    if (cfg->queue_size == 0) return TEST_SKIP;

    struct vring cq;
    if (vring_alloc(&cq, 16) < 0) return TEST_SKIP;
    vring_attach(dev, &cq, 2);

    uint8_t *p = vv_alloc_pages(1);
    struct virtio_console_control *m = (void *)p;
    struct virtio_console_resize *r = (void *)(p + sizeof(*m));
    m->id = 0; m->event = VIRTIO_CONSOLE_RESIZE; m->value = 0;
    r->rows = 80; r->cols = 24;

    vring_raw_set_desc(&cq, 0, vv_virt_to_phys(p),
                       (uint32_t)(sizeof(*m) + sizeof(*r)), 0, 0);
    vring_raw_set_avail(&cq, 0, 0);
    vring_raw_set_avail_idx(&cq, 1);

    return vv_kick_and_wait(dev, &cq, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST(C0047, VIRTIO_PCI_DEVICE_CONSOLE, test,
              "MULTIPORT RESIZE event for port 0",
              VIRTIO_SPEC_V1_4, "5.3.6.1");
