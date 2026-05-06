/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0013: Console receive with NEXT pointing past the ring.
 *
 * Build an RX chain whose head sets VRING_DESC_F_NEXT|WRITE and
 * references descriptor index 0xFFFE, far outside the ring. The
 * device must reject the chain rather than dereference an
 * out-of-bounds slot when host input arrives.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_console_rx_oob_next(struct virtio_dev *dev,
                                              struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    uint64_t buf_phys = vv_virt_to_phys(buf);

    vring_raw_set_desc(vr, 0, buf_phys, 32,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 0xFFFE);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(C0013, VIRTIO_PCI_DEVICE_CONSOLE, test_console_rx_oob_next,
              "Console receive NEXT pointing past ring",
              VIRTIO_SPEC_V1_2, "5.3");
