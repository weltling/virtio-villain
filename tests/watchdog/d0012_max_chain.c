/* SPDX-License-Identifier: Apache-2.0 */
/*
 * D0012: Watchdog max length writable chain.
 *
 * Build a chain of writable descriptors that walks every slot in
 * the queue using NEXT links, ending at the last slot. The device
 * must walk the chain bounded by ring size and respond once.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_watchdog_max_chain(struct virtio_dev *dev,
                                             struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    uint64_t buf_phys = vv_virt_to_phys(buf);

    uint16_t n = vr->size;
    for (uint16_t i = 0; i < n; i++) {
        uint16_t flags = VRING_DESC_F_WRITE;
        uint16_t next  = 0;
        if (i + 1 < n) {
            flags |= VRING_DESC_F_NEXT;
            next   = (uint16_t)(i + 1);
        }
        vring_raw_set_desc(vr, i, buf_phys + i * 4, 1, flags, next);
    }

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(D0012, VIRTIO_PCI_DEVICE_WATCHDOG, test_watchdog_max_chain,
              "Watchdog ring length writable chain",
              VIRTIO_SPEC_V1_2, "2.7.5");
