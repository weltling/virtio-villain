/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0098: PCI BAR region sizes are non zero.
 *
 * Spec 4.1.4: The device exposes capabilities in specific BAR
 * regions. After init, the notify_length and device_cfg_length
 * fields populated during capability walk must be non zero,
 * confirming the BARs are properly sized.
 */
#include "lib/virtio_test.h"

static test_result_t test_pci_bar_sizes(struct virtio_dev *dev)
{
    if (dev->notify_length == 0)
        TFAIL("notify_length is 0");

    if (!dev->device_cfg)
        return TEST_SKIP;  /* some devices may lack device config */

    if (dev->device_cfg_length == 0)
        TFAIL("device_cfg_length is 0 despite device_cfg mapped");

    return TEST_PASS;
}

REGISTER_TEST_DEV(PCI0098, VIRTIO_PCI_DEVICE_BLK, test_pci_bar_sizes,
                  "PCI BAR notify and device config regions are sized",
                  VIRTIO_SPEC_V1_2, "4.1.4");
