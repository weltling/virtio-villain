/* SPDX-License-Identifier: Apache-2.0 */
/*
 * D0014: watchdog_zero_len_chain
 *
 * Submit a chain of seventeen zero length writable descriptors.
 * Spec 5.16 expects the watchdog to consume a single writable
 * descriptor per ping; this malformed chain must not be treated
 * as a ping nor reset the host.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_watchdog_zero_chain(struct virtio_dev *dev,
                                              struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    uint64_t phys = vv_virt_to_phys(buf);

    for (int i = 0; i < 16; i++) {
        vring_raw_set_desc(vr, (uint16_t)i, phys, 0,
                           VRING_DESC_F_WRITE | VRING_DESC_F_NEXT,
                           (uint16_t)(i + 1));
    }
    vring_raw_set_desc(vr, 16, phys, 0, VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(D0014, VIRTIO_PCI_DEVICE_WATCHDOG, test_watchdog_zero_chain,
              "Chain of seventeen zero length descriptors",
              VIRTIO_SPEC_V1_4, "-");
