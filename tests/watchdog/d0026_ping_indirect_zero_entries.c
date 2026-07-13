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
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_watchdog_indirect_zero(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_F_INDIRECT_DESC)))
        return TEST_SKIP;

    /* Indirect descriptor with len=0 (no entries reachable) */
    void *table = vv_alloc_pages(1);
    memset(table, 0, 4096);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(table), 0,
                       VRING_DESC_F_INDIRECT, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_REQUIRES(D0026, VIRTIO_PCI_DEVICE_WATCHDOG, test_watchdog_indirect_zero,
              "Watchdog ping with zero length indirect table",
              VIRTIO_SPEC_V1_2, "2.7.7",
              (1ULL << VIRTIO_F_INDIRECT_DESC), 0);
