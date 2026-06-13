/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0015: Fill the console receive ring completely.
 *
 * Post a full ring of writable receiveq descriptors and kick. The
 * device must accept the whole batch without crashing. With no host
 * input pending, the expected outcome is REJECT (no used entries).
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_console_rx_fill_ring(struct virtio_dev *dev,
                                               struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    uint64_t buf_phys = vv_virt_to_phys(buf);

    for (uint16_t i = 0; i < vr->size; i++) {
        vring_raw_set_desc(vr, i, buf_phys + (i * 32), 32,
                           VRING_DESC_F_WRITE, 0);
        vring_raw_set_avail(vr, i, i);
    }
    vring_raw_set_avail_idx(vr, vr->size);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(C0015, VIRTIO_PCI_DEVICE_CONSOLE, test_console_rx_fill_ring,
              "Console receive ring fully populated",
              VIRTIO_SPEC_V1_2, "5.3");
