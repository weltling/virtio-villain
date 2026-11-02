/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0001: Console basic transmit.
 *
 * Submit a single read-only descriptor with a small payload to the
 * transmitq (queue 1). The device must consume the descriptor and
 * advance the used ring without crashing.
 *
 * Cloud Hypervisor console: 2 queues, single-port mode.
 *   queue 0 = receiveq (device writes), queue 1 = transmitq.
 */
#include "lib/virtio_test.h"

static test_result_t test_console_tx_basic(struct virtio_dev *dev,
                                           struct vring *vr)
{
    char *buf = (char *)vv_alloc_pages(1);
    memcpy(buf, "vv\n", 3);

    /* TX descriptor: read-only payload sent from guest to device */
    vv_desc(vr, 0, buf, 3, 0, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_Q(C0001, VIRTIO_PCI_DEVICE_CONSOLE, test_console_tx_basic,
                "Console basic transmit",
                VIRTIO_SPEC_V1_2, "5.3", 1);
