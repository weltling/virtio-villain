/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0048: queue_size readback after write
 *
 * Spec 4.1.4.3.2 says queue_size for the selected queue is driver
 * writable up to the maximum the device advertises. After writing
 * a power of two within range the read must return that value. The
 * device must not silently rewrite the field to its preferred size.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

static test_result_t test_queue_size_readback(struct virtio_dev *dev,
                                              struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    if (virtio_pci_init(dev) != 0)
        TFAIL("Failed to init device");

    cfg->queue_select = 0;
    __sync_synchronize();
    uint16_t qmax = cfg->queue_size;
    if (qmax == 0)
        return TEST_SKIP;

    static const uint16_t sizes[] = {2, 4, 8, 16, 32, 64};

    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        if (sizes[i] > qmax)
            continue;
        cfg->queue_size = sizes[i];
        __sync_synchronize();
        uint16_t v = cfg->queue_size;
        if (v != sizes[i])
            TFAIL("v != sizes[i]");
    }

    /* Restore */
    cfg->queue_size = qmax;
    __sync_synchronize();

    return TEST_PASS;
}

REGISTER_TEST(S0048, VIRTIO_PCI_DEVICE_BLK, test_queue_size_readback,
              "queue_size reads back driver written value",
              VIRTIO_SPEC_V1_2, "4.1.4.3.2");
