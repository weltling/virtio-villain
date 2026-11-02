/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0044: MULTIPORT PORT_READY message.
 *
 * Spec 5.3.6.1: after DEVICE_READY the driver acknowledges
 * each port the device adds with PORT_READY. Send PORT_READY
 * for port id 0 on the control queue.
 */
#include "lib/virtio_test.h"

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    (void)vr;

    if (!virtio_pci_feature_offered(dev, VIRTIO_CONSOLE_F_MULTIPORT))
        return TEST_SKIP;

    cfg->queue_select = 2;
    __sync_synchronize();
    if (cfg->queue_size == 0) return TEST_SKIP;

    struct vring cq;
    if (vring_alloc(&cq, 16) < 0) return TEST_SKIP;
    vring_attach(dev, &cq, 2);

    struct virtio_console_control *m = vv_alloc_pages(1);
    m->id = 0;
    m->event = VIRTIO_CONSOLE_PORT_READY;
    m->value = 1;

    vv_desc(&cq, 0, m, sizeof(*m), 0, 0);
    vring_raw_set_avail(&cq, 0, 0);
    vring_raw_set_avail_idx(&cq, 1);

    return vv_kick_and_wait(dev, &cq, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST_REQUIRES(C0044, VIRTIO_PCI_DEVICE_CONSOLE, test,
              "MULTIPORT PORT_READY for port 0",
              VIRTIO_SPEC_V1_4, "5.3.6.1",
              (1ULL << VIRTIO_CONSOLE_F_MULTIPORT), 0);
