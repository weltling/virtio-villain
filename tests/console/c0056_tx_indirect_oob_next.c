/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0056: tx_indirect_oob_next
 *
 * With VIRTIO_F_INDIRECT_DESC negotiated the first entry of a transmit
 * indirect table links to a next index beyond the entries the table
 * holds. The device must refuse the out of range link without harm.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_console_tx_indirect_oob_next(struct virtio_dev *dev,
                                                       struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_F_INDIRECT_DESC))
        return TEST_SKIP;

    struct vring_desc *itab = vv_alloc_pages(1);
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0x41, 64);

    itab[0].addr = vv_virt_to_phys(buf);
    itab[0].len = 64;
    itab[0].flags = VRING_DESC_F_NEXT;
    itab[0].next = 99;      /* beyond the single entry table */

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(itab),
                       sizeof(struct vring_desc), VRING_DESC_F_INDIRECT, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q_REQUIRES(C0056, VIRTIO_PCI_DEVICE_CONSOLE,
                         test_console_tx_indirect_oob_next,
                         "Console transmit indirect entry next out of bounds",
                         VIRTIO_SPEC_V1_2, "2.7.7", 1,
                         (1ULL << VIRTIO_F_INDIRECT_DESC), 0);
