/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0201: device survives a host snapshot taken while MSI-X
 * vectors are masked.
 *
 * Spec 4.1.5.1.4 lets the driver write VIRTIO_MSI_NO_VECTOR
 * (0xFFFF) into msix_config and queue_msix_vector to disable
 * vector delivery without disabling MSI-X itself. The driver here
 * masks both, the sidecar pauses, snapshots, and resumes the VM,
 * the driver restores the prior vectors. The device must not
 * raise FAILED or NEEDS_RESET across the window.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>


static test_result_t test_pci_snapshot_masked_msix(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    if (!(cfg->device_status & VIRTIO_STATUS_DRIVER_OK))
        return TEST_SKIP;

    uint16_t saved_cfg = cfg->msix_config;
    cfg->queue_select = 0;
    __sync_synchronize();
    uint16_t saved_q = cfg->queue_msix_vector;

    cfg->msix_config = VIRTIO_MSI_NO_VECTOR;
    cfg->queue_msix_vector = VIRTIO_MSI_NO_VECTOR;
    __sync_synchronize();

    int waited = 0;
    test_result_t verdict = TEST_PASS;
    while (waited < 8000) {
        __sync_synchronize();
        uint8_t st = cfg->device_status;
        if (st & VIRTIO_STATUS_FAILED) {
            verdict = TEST_FAIL;
            break;
        }
        if (st & VIRTIO_STATUS_NEEDS_RESET) {
            verdict = TEST_FAIL;
            break;
        }
        usleep(100 * 1000);
        waited += 100;
    }

    cfg->msix_config = saved_cfg;
    cfg->queue_msix_vector = saved_q;
    __sync_synchronize();

    if (verdict != TEST_PASS)
        return verdict;
    if (!(cfg->device_status & VIRTIO_STATUS_DRIVER_OK))
        TFAIL("!(cfg->device_status & VIRTIO_STATUS_DRIVER_OK)");
    return TEST_PASS;
}

REGISTER_TEST(PCI0201, VIRTIO_PCI_DEVICE_BLK,
              test_pci_snapshot_masked_msix,
              "device survives a snapshot with MSI-X vectors masked",
              VIRTIO_SPEC_V1_2, "4.1.5.1.4");
