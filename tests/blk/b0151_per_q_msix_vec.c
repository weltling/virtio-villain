/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0151: per queue MSI-X vectors are independent
 *
 * Spec 4.1.5.1 says queue_msix_vector binds a vector to a queue.
 * Two queues bound to two distinct vectors must each have their
 * binding readable back from queue_msix_vector after writing.
 * Bind queue 0 to vector 1 and queue 1 to vector 2, read each
 * back via queue_select to confirm the device stored both
 * independently, then restore both to NO_VECTOR.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <unistd.h>


static test_result_t test_blk_per_q_msix_vec(struct virtio_dev *dev,
                                             struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    if (cfg->num_queues < 2)
        return TEST_SKIP;

    cfg->queue_select = 0;
    __sync_synchronize();
    cfg->queue_msix_vector = 1;
    __sync_synchronize();
    uint16_t got0 = cfg->queue_msix_vector;

    cfg->queue_select = 1;
    __sync_synchronize();
    cfg->queue_msix_vector = 2;
    __sync_synchronize();
    uint16_t got1 = cfg->queue_msix_vector;

    /* Read back queue 0 again and confirm vector did not bleed */
    cfg->queue_select = 0;
    __sync_synchronize();
    uint16_t back0 = cfg->queue_msix_vector;

    /* Restore */
    cfg->queue_msix_vector = VIRTIO_MSI_NO_VECTOR;
    __sync_synchronize();
    cfg->queue_select = 1;
    __sync_synchronize();
    cfg->queue_msix_vector = VIRTIO_MSI_NO_VECTOR;
    __sync_synchronize();

    /* Device may decline a vector by reverting to NO_VECTOR */
    if (got0 == VIRTIO_MSI_NO_VECTOR || got1 == VIRTIO_MSI_NO_VECTOR)
        TREJECT("got0 == VIRTIO_MSI_NO_VECTOR || got1 == VIRTIO_MSI_NO_VECTOR");
    if (got0 != 1 || got1 != 2 || back0 != 1)
        TFAIL("got0 != 1 || got1 != 2 || back0 != 1");

    return TEST_PASS;
}

REGISTER_TEST(B0151, VIRTIO_PCI_DEVICE_BLK, test_blk_per_q_msix_vec,
              "queue_msix_vector stored independently per queue",
              VIRTIO_SPEC_V1_2, "4.1.5.1");
