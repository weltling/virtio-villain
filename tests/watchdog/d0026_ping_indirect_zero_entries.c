/* SPDX-License-Identifier: Apache-2.0 */
/*
 * D0026: Watchdog ping where the avail entry points at an
 * INDIRECT descriptor whose table length is zero.
 *
 * Spec 2.7.7: an indirect descriptor table must have at least
 * one entry. Submit a single avail entry pointing at a zero
 * length indirect descriptor. The device must reject the chain
 * and must not fall back to consuming the next descriptor.
 */
#include "lib/virtio_test.h"

static test_result_t test_watchdog_indirect_zero(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_F_INDIRECT_DESC))
        return TEST_SKIP;

    /* Indirect descriptor with len=0 (no entries reachable) */
    void *table = vv_alloc_pages(1);

    vv_desc(vr, 0, table, 0, VRING_DESC_F_INDIRECT, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_REQUIRES(D0026, VIRTIO_PCI_DEVICE_WATCHDOG, test_watchdog_indirect_zero,
              "Watchdog ping with zero length indirect table",
              VIRTIO_SPEC_V1_2, "2.7.7",
              (1ULL << VIRTIO_F_INDIRECT_DESC), 0);
