/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0048: MULTIPORT PORT_OPEN with value 1 then 0.
 *
 * Spec 5.3.6.1: the driver opens and closes a port with
 * PORT_OPEN value=1 followed by PORT_OPEN value=0. Both
 * messages must be consumed.
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

    cfg->queue_select = 2;
    __sync_synchronize();
    if (cfg->queue_size == 0) return TEST_SKIP;

    struct vring cq;
    if (vring_alloc(&cq, 16) < 0) return TEST_SKIP;
    vring_attach(dev, &cq, 2);

    struct virtio_console_control *open_msg  = vv_alloc_pages(1);
    struct virtio_console_control *close_msg = vv_alloc_pages(1);
    open_msg->id  = 0; open_msg->event  = VIRTIO_CONSOLE_PORT_OPEN; open_msg->value  = 1;
    close_msg->id = 0; close_msg->event = VIRTIO_CONSOLE_PORT_OPEN; close_msg->value = 0;

    vring_raw_set_desc(&cq, 0, vv_virt_to_phys(open_msg), sizeof(*open_msg), 0, 0);
    vring_raw_set_avail(&cq, 0, 0);
    vring_raw_set_avail_idx(&cq, 1);
    test_result_t r = vv_kick_and_wait(dev, &cq, 2, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;

    vring_raw_set_desc(&cq, 1, vv_virt_to_phys(close_msg), sizeof(*close_msg), 0, 0);
    vring_raw_set_avail(&cq, 1, 1);
    vring_raw_set_avail_idx(&cq, 2);
    return vv_kick_and_wait(dev, &cq, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST(C0048, VIRTIO_PCI_DEVICE_CONSOLE, test,
              "MULTIPORT PORT_OPEN then PORT_CLOSE",
              VIRTIO_SPEC_V1_4, "5.3.6.1");
