/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0022: Console transmit chain at queue_size descriptors.
 *
 * Build a TX chain that uses every slot in the ring (queue_size
 * descriptors) linked via VRING_DESC_F_NEXT. The device must
 * walk the full chain without overflowing internal limits.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_console_tx_max_chain(struct virtio_dev *dev,
                                               struct vring *vr)
{
    char *buf = (char *)vv_alloc_pages(1);
    memset(buf, 'v', 256);
    uint64_t buf_phys = vv_virt_to_phys(buf);

    uint16_t n = vr->size;
    for (uint16_t i = 0; i < n; i++) {
        uint16_t flags = (i + 1 < n) ? VRING_DESC_F_NEXT : 0;
        uint16_t next  = (i + 1 < n) ? (uint16_t)(i + 1) : 0;
        vring_raw_set_desc(vr, i, buf_phys + i, 1, flags, next);
    }
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(C0022, VIRTIO_PCI_DEVICE_CONSOLE, test_console_tx_max_chain,
                "Console transmit max-length chain",
                VIRTIO_SPEC_V1_2, "5.3", 1);
