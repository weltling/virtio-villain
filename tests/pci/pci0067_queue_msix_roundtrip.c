/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0067: queue_msix_vector readback returns written or NO_VECTOR
 *
 * Spec 4.1.4.3.2 defines queue_msix_vector as a per queue field
 * the driver sets to bind a queue to an MSI-X vector. On read
 * back the driver must accept either the value written, signaling
 * success, or VIRTIO_MSI_NO_VECTOR, signaling that the device
 * could not allocate the vector. Any other value is malformed.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

#define VIRTIO_MSI_NO_VECTOR 0xFFFF

static test_result_t test_pci_queue_msix_roundtrip(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    /* Reset so writes to per queue fields cannot disrupt live IO */
    cfg->device_status = 0;
    __sync_synchronize();
    int rtries = 200;
    while (rtries-- > 0 && cfg->device_status != 0)
        usleep(1000);
    if (cfg->device_status != 0)
        TWEDGED("cfg->device_status != 0");

    uint16_t nq = cfg->num_queues;
    if (nq == 0 || nq > 32)
        return TEST_SKIP;

    static const uint16_t vals[] = {0, 1, VIRTIO_MSI_NO_VECTOR};

    for (uint16_t q = 0; q < nq; q++) {
        cfg->queue_select = q;
        __sync_synchronize();
        if (cfg->queue_size == 0)
            continue;
        for (size_t i = 0; i < sizeof(vals) / sizeof(vals[0]); i++) {
            cfg->queue_msix_vector = vals[i];
            __sync_synchronize();
            uint16_t v = cfg->queue_msix_vector;
            if (v != vals[i] && v != VIRTIO_MSI_NO_VECTOR) {
                cfg->queue_msix_vector = VIRTIO_MSI_NO_VECTOR;
                TFAIL("v != vals[i] && v != VIRTIO_MSI_NO_VECTOR");
            }
        }
        cfg->queue_msix_vector = VIRTIO_MSI_NO_VECTOR;
        __sync_synchronize();
    }

    cfg->queue_select = 0;
    __sync_synchronize();
    return TEST_PASS;
}

REGISTER_TEST(PCI0067, VIRTIO_PCI_DEVICE_BLK, test_pci_queue_msix_roundtrip,
              "queue_msix_vector readback equals value or NO_VECTOR",
              VIRTIO_SPEC_V1_2, "4.1.4.3.2");
