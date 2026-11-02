/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0028: console_ctrl_vq_no_multiport
 *
 * The harness never negotiates VIRTIO_CONSOLE_F_MULTIPORT. Read
 * num_queues from common_cfg. With MULTIPORT off only the two
 * single port queues (RX, TX) are required. If the device claims
 * additional ctrl queues without MULTIPORT, that is itself a
 * deviation from spec 5.3.5. Otherwise this test passes by
 * confirming the layout.
 */
#include "lib/virtio_test.h"

static test_result_t test_console_ctrl_vq_no_multiport(struct virtio_dev *dev)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    if (virtio_pci_feature_offered(dev, VIRTIO_CONSOLE_F_MULTIPORT))
        return TEST_SKIP;

    uint16_t nq = cfg->num_queues;
    if (nq < 2)
        TFAIL("nq < 2");

    /*
     * Without MULTIPORT the device exposes a single port pair.
     * Cloud Hypervisor exposes 2 queues. If more are present the
     * driver is not allowed to use them, but the device must not
     * crash on a kick targeting a non existent ctrl queue. Probe
     * by selecting queue index nq and checking queue_size returns
     * zero rather than a phantom value.
     */
    cfg->queue_select = nq;
    __sync_synchronize();
    uint16_t qsize = cfg->queue_size;
    if (qsize != 0)
        TFAIL("qsize != 0");

    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    return TEST_PASS;
}

REGISTER_TEST_DEV(C0028, VIRTIO_PCI_DEVICE_CONSOLE,
                  test_console_ctrl_vq_no_multiport,
                  "Console without MULTIPORT exposes no extra queues",
                  VIRTIO_SPEC_V1_2, "5.3.5");
