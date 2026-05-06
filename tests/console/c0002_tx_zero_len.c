/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0002: Console transmit with zero-length descriptor.
 *
 * Submit a read-only descriptor of length 0 to the transmitq. The
 * device must handle the empty payload edge case without crashing.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_console_tx_zero_len(struct virtio_dev *dev,
                                              struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    uint64_t buf_phys = vv_virt_to_phys(buf);

    vring_raw_set_desc(vr, 0, buf_phys, 0, 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(C0002, VIRTIO_PCI_DEVICE_CONSOLE, test_console_tx_zero_len,
                "Console transmit zero-length descriptor",
                VIRTIO_SPEC_V1_2, "5.3", 1);
