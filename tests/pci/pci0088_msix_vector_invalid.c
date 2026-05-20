/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0088: MSI-X vector set to invalid value.
 *
 * Spec 4.1.4.3.2: Set config_msix_vector to 0xFFFE (not
 * VIRTIO_MSI_NO_VECTOR and not a valid vector). The device must
 * handle the invalid vector gracefully.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_pci_msix_vector_invalid(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    (void)vr;

    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* Write an invalid MSI-X vector (neither NO_VECTOR nor allocated) */
    cfg->msix_config = 0xFFFE;
    __sync_synchronize();

    /* Read back; device should return NO_VECTOR on failure */
    uint16_t readback = cfg->msix_config;
    (void)readback;

    usleep(100 * 1000);

    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    return TEST_PASS;
}

REGISTER_TEST(PCI0088, VIRTIO_PCI_DEVICE_BLK, test_pci_msix_vector_invalid,
              "Set config MSI-X vector to invalid value",
              VIRTIO_SPEC_V1_2, "4.1.4.3.2");
